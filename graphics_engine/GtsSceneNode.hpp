#pragma once

#include <vector>
#include <string>
#include <cmath>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/string_cast.hpp> 

#include "GtsRenderableObject.hpp"
#include "GtsAnimation.hpp"
#include "GtsSceneNodeTransformEvent.hpp"

enum LastTransform { translation, rotation, scaling };

class GtsSceneNode {
private:
    std::string identifier;

    glm::vec3 positionVector;
    glm::vec3 rotationVector;
    glm::vec3 scaleVector;

    glm::vec3 lastTransform;
    LastTransform lastTransformType;

    glm::mat4 translationMatrix;
    glm::mat4 rotationMatrix;
    glm::mat4 scaleMatrix;

    std::vector<GtsSceneNode*> children;
    GtsSceneNode* parent;

    GtsRenderableObject* renderableObject;
    GtsAnimation* animation;

    GtsSceneNodeTransformEvent transformEvent;

    bool isActive;
    bool isAnimationActive;

    void updateMatrices() {
        translationMatrix = glm::translate(glm::mat4(1.0f), positionVector);

        glm::quat quatX = glm::angleAxis(glm::radians(rotationVector.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat quatY = glm::angleAxis(glm::radians(rotationVector.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat quatZ = glm::angleAxis(glm::radians(rotationVector.z), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::quat combinedQuat = quatZ * quatY * quatX;
        rotationMatrix = glm::toMat4(combinedQuat);

        scaleMatrix = glm::scale(glm::mat4(1.0f), scaleVector);
    }

public:
    GtsSceneNode()
        : positionVector(0.0f), rotationVector(0.0f), scaleVector(1.0f),
        translationMatrix(1.0f), rotationMatrix(1.0f), scaleMatrix(1.0f),
        parent(nullptr), renderableObject(nullptr), animation(nullptr),
        isActive(true), isAnimationActive(false) {}

    GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim) : GtsSceneNode() {
        renderableObject = obj;
        animation = anim;
        isAnimationActive = true;
    }

    GtsSceneNode(GtsRenderableObject* obj, GtsAnimation* anim, std::string identifier) : GtsSceneNode(obj, anim) {
        this->identifier = identifier;
    }

    ~GtsSceneNode() {
        delete renderableObject;
        delete animation;
        for (auto child : children) {
            delete child;
        }
    }

    void subscribeToTransformEvent(std::function<void()> f) {
        transformEvent.subscribe(f);
    }

    void setActive(bool active) {
        isActive = active;
    }

    bool getActive() const {
        return isActive;
    }

    void enableAnimation()
    {
        isAnimationActive = true;
    }

    void disableAnimation()
    {
        isAnimationActive = false;
    }

    std::vector<GtsSceneNode*> getChildren()
    {
        return children;
    }

    GtsSceneNode* getParent()
    {
        return parent;
    }

    void undoLastTransform() {
        switch (lastTransformType) {
        case translation:
            positionVector -= lastTransform;
            break;
        case rotation:
            rotationVector -= lastTransform;
            break;
        case scaling:
            // Not supported
            break;
        }
        updateMatrices();
    }

    void translate(const glm::vec3& offset, std::string debug) 
    {
        if (!isActive) return;

        //std::cout << "translate call by " << debug << std::endl;
        lastTransform = offset;
        lastTransformType = translation;

        positionVector += offset;
        transformEvent.notify();
    }

    void rotate(const glm::vec3& rot) {
        if (!isActive) return;

        lastTransform = rot;
        lastTransformType = rotation;

        rotationVector += rot;
        std:: cout << glm::to_string(rotationVector) << std::endl;
        transformEvent.notify();
    }

    void scale(const glm::vec3& scl) {
        if (!isActive) return;

        lastTransform = scl;
        lastTransformType = scaling;

        scaleVector *= scl;
        transformEvent.notify();
    }

    void addChild(GtsSceneNode* child) {
        child->parent = this;
        children.push_back(child);
    }

    glm::mat4 getModelMatrix()
    {
        return translationMatrix * rotationMatrix * scaleMatrix;
    }

    glm::vec3 getWorldPosition() 
    {

        glm::mat4 modelMatrix = glm::mat4(1.0f);
        GtsSceneNode* currentNode = this;

        while (currentNode) {
            currentNode->updateMatrices();  // Ensure matrices are updated
            modelMatrix =  currentNode->getModelMatrix() * modelMatrix;
            currentNode = currentNode->parent;
        }

        glm::vec4 worldPosition = modelMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // Use homogeneous coordinates
        std:: cout << "World Position: " << glm::to_string(worldPosition) << std::endl;
        return glm::vec3(worldPosition.x, worldPosition.y, worldPosition.z);
    }

    glm::ivec3 worldPositionToGrid(const glm::vec3& worldPos, float gridCellSize) {
        int gridX = std::round(worldPos.x / gridCellSize);
        int gridY = std::round(worldPos.y / gridCellSize);
        int gridZ = std::round(worldPos.z / gridCellSize);
        return glm::ivec3(gridX, gridY, gridZ);
    }

    std::vector<glm::ivec3> getGridCoordinates() {
        std::vector<glm::ivec3> gridCoordinates;
        for (auto child : children) {
            glm::vec3 worldPos = child->getWorldPosition();
            glm::ivec3 gridPos = worldPositionToGrid(worldPos, 1.0f);
            gridCoordinates.push_back(gridPos);
        }
        return gridCoordinates;
    }

    void update(const glm::mat4& parentTransform, GtsCamera& camera, int framesInFlight, float deltaTime) {
        if (!isActive) return;
        updateMatrices();

        if ((animation != nullptr) && isAnimationActive) 
        {
            animation->animate(this, deltaTime);
        }

        glm::mat4 modelMatrix = parentTransform * translationMatrix * rotationMatrix * scaleMatrix;

        if (renderableObject) {
            renderableObject->updateUniforms(modelMatrix, camera, framesInFlight);
        }

        for (auto child : children) {
            child->update(modelMatrix, camera, framesInFlight, deltaTime);
        }
    }

    void draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame) {
        if (renderableObject) {
            renderableObject->draw(commandBuffer, pipelineLayout, currentFrame);
        }

        for (auto child : children) {
            child->draw(commandBuffer, pipelineLayout, currentFrame);
        }
    }

    GtsSceneNode* search(const std::string& identifier) {
        if (this->identifier == identifier) {
            return this;
        }

        for (auto child : children) {
            auto res = child->search(identifier);
            if (res != nullptr) {
                return res;
            }
        }

        return nullptr;
    }
};
