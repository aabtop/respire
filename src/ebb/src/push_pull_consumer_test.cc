#include <cmath>
#include <cstring>
#include <gtest/gtest.h>

#include "ebbpp.h"

class PushPullConsumerTests : public ::testing::TestWithParam<int32_t> {};

const size_t kFiberStackSize = 16 * 1024;

TEST_P(PushPullConsumerTests, CanSendSimpleRequests) {
  ebb::Environment env(GetParam(), kFiberStackSize);

  int internal_counter = 0;
  ebb::PushPullConsumer<int(int)> increment_node(
      &env, [&internal_counter](int add_amount) {
    internal_counter += add_amount;
    return internal_counter;
  });

  ebb::Future<int> increment_request1(&increment_node, 5);
  EXPECT_EQ(5, *increment_request1.GetValue());
  EXPECT_EQ(5, internal_counter);

  ebb::Future<int> increment_request2(&increment_node, 6);
  EXPECT_EQ(11, *increment_request2.GetValue());
  EXPECT_EQ(11, internal_counter);
}

class CounterNode {
 public:
  typedef ebb::Future<int> Future;

  CounterNode(ebb::Environment* env, const std::function<int(int&&)>& function)
      : request_reply_node_(env, function) {}
  virtual ~CounterNode() {}

  ebb::PushPullConsumer<int(int)>* request_reply_node() {
    return &request_reply_node_;
  };

 private:
  ebb::PushPullConsumer<int(int)> request_reply_node_;
};

class CounterInternalNode : public CounterNode {
 public:
  CounterInternalNode(ebb::Environment* env,
                      const std::vector<CounterNode*>& dependencies)
      : CounterNode(env, [this](int i) { return HandleRequest(i); }),
        x_(0), dependencies_(dependencies) {}

  int x() const { return x_; }

 private:
  int HandleRequest(int i) {
    std::vector<std::unique_ptr<CounterNode::Future>> futures;
    for (auto dependency : dependencies_) {
      futures.emplace_back(new CounterNode::Future(
          dependency->request_reply_node(), i));
    }

    x_ += i;

    int x = x_;

    for (auto& future : futures) {
      x += *future->GetValue();
    }

    return x;
  }

  int x_;
  std::vector<CounterNode*> dependencies_;
};

class CounterLeafNode : public CounterNode {
 public:
  CounterLeafNode(ebb::Environment* env)
      : CounterNode(env, [this](int i) { return HandleRequest(i); }), x_(0) {}

  int x() const { return x_; }

 private:
  int HandleRequest(int i) {
    x_ += i;
    return x_;
  }

  int x_;
};

// Tests that we can pass messages through a chain of dependencies, and pull
// a value back out of the root.
TEST_P(PushPullConsumerTests, CanSendChainedDependencyRequests) {
  ebb::Environment env(GetParam(), kFiberStackSize);

  const int kNumInternalNodes = 20;
  const int kInitialParameter = 1;

  CounterLeafNode leaf_node(&env);
  std::vector<std::unique_ptr<CounterInternalNode>> internal_nodes;

  for (int i = 0; i < kNumInternalNodes; ++i) {
    CounterNode* previous_node = &leaf_node;
    if (i > 0) {
      previous_node = internal_nodes[i - 1].get();
    }

    internal_nodes.emplace_back(new CounterInternalNode(&env, {previous_node}));
  }

  CounterNode::Future pull1(internal_nodes.back()->request_reply_node(),
                            kInitialParameter);
  EXPECT_EQ(kInitialParameter * (kNumInternalNodes + 1), *pull1.GetValue());

  // Check that the state accumulated properly for the second pull
  CounterNode::Future pull2(internal_nodes.back()->request_reply_node(),
                            0);
  EXPECT_EQ(kInitialParameter * (kNumInternalNodes + 1) , *pull2.GetValue());
}

// Tests that we can pass messages through a tree of dependencies, and pull
// a value back out of the root.
TEST_P(PushPullConsumerTests, CanSendDependencyTreeRequests) {
  ebb::Environment env(GetParam(), kFiberStackSize);

  const int kTreeHeight = 5;
  const int kTreeChildrenPerNode = 4;
  const int kInitialParameter = 1;

  typedef std::vector<std::unique_ptr<CounterNode>> NodeLayer;
  std::vector<NodeLayer> tree_layers;
  for (int i = 0; i < kTreeHeight; ++i) {
    int num_nodes_in_layer = static_cast<int>(std::round(
        std::pow(static_cast<float>(kTreeChildrenPerNode),
                 static_cast<float>(kTreeHeight - i - 1))));

    tree_layers.push_back(NodeLayer());
    if (i == 0) {
      for (int j = 0; j < num_nodes_in_layer; ++j) {
        tree_layers.back().emplace_back(new CounterLeafNode(&env));
      }
    } else {
      for (int j = 0; j < num_nodes_in_layer; ++j) {
        std::vector<CounterNode*> children;
        const NodeLayer* previous_layer = &tree_layers[tree_layers.size() - 2];
        NodeLayer::const_iterator children_start =
            previous_layer->begin() + j * kTreeChildrenPerNode;
        std::transform(
            children_start, children_start + kTreeChildrenPerNode,
            std::back_inserter(children),
            [](const std::unique_ptr<CounterNode>& x) { return x.get(); });

        tree_layers.back().emplace_back(new CounterInternalNode(
            &env, children));
      }
    }
  }

  ASSERT_EQ(1, tree_layers.back().size());
  CounterNode* root_node = tree_layers.back()[0].get();

  size_t num_nodes = 0;
  for (const auto& layer : tree_layers) {
    num_nodes += layer.size();
  }

  CounterNode::Future pull1(root_node->request_reply_node(), kInitialParameter);
  EXPECT_EQ(num_nodes, *pull1.GetValue());

  // Check that the state accumulated properly for the second pull
  CounterNode::Future pull2(root_node->request_reply_node(), 0);
  EXPECT_EQ(num_nodes, *pull2.GetValue());
}

TEST_P(PushPullConsumerTests, CanSendVoidParamRequests) {
  ebb::Environment env(GetParam(), kFiberStackSize);

  int internal_counter = 0;
  ebb::PushPullConsumer<int()> increment_node(
      &env, [&internal_counter]() {
    internal_counter += 1;
    return internal_counter;
  });

  ebb::Future<int> increment_request1(&increment_node);
  EXPECT_EQ(1, *increment_request1.GetValue());
  EXPECT_EQ(1, internal_counter);

  ebb::Future<int> increment_request2(&increment_node);
  EXPECT_EQ(2, *increment_request2.GetValue());
  EXPECT_EQ(2, internal_counter);
}

TEST_P(PushPullConsumerTests, CanDestroyFutureBeforeCallingGetValue) {
  int internal_counter = 0;
  {
    ebb::Environment env(GetParam(), kFiberStackSize);

    ebb::PushPullConsumer<int()> consumer1(
        &env, [&internal_counter]() {
      internal_counter += 1;
      return internal_counter;
    });

    ebb::PushPullConsumer<int()> consumer2(
        &env, [&consumer1]() {
      ebb::Future<int> increment_request2(&consumer1);
      return 0;
    });

    ebb::Future<int> increment_request1(&consumer2);
  }

  EXPECT_EQ(1, internal_counter);
}

INSTANTIATE_TEST_CASE_P(
    VaryingPushPullConsumerParams, PushPullConsumerTests,
    ::testing::Values(1, 2, 8, 100));
