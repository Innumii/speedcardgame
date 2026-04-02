// abstract interface for animations
// all animations should inherit from this class and implement the update and isFinished methods
#ifndef ANIMATION_INTERFACE_HPP
#define ANIMATION_INTERFACE_HPP

class AnimationInterface {
public:
    virtual ~AnimationInterface() = default;
    virtual void start() {}
    virtual void update(float dt) = 0;
    virtual bool isFinished() const = 0;
    
    virtual bool isBlocking() const { return true; }

};

#endif