// class to group animations that should run together
#ifndef ANIMATION_GROUP_HPP
#define ANIMATION_GROUP_HPP

#include "AnimationInterface.hpp"
#include <memory>
#include <vector>

class AnimationGroup : public AnimationInterface {
    std::vector<std::shared_ptr<AnimationInterface>> animations;

public:
    void add(std::shared_ptr<AnimationInterface> animation);
    const std::vector<std::shared_ptr<AnimationInterface>>& getAnimations() const;
    void start() override;
    void update(float dt) override;
    bool isFinished() const override;
};

#endif