#pragma once

#include "material/TestMaterialResources.h"

class CameraContractResources : public Resources
{
    public:
    view_id_type requestCameraBuffer() override;
    void         releaseCameraBuffer(view_id_type view) override;
    void         uploadCameraView(view_id_type view, const glm::mat4& eye, const glm::mat4& projection) override;

    unsigned  allocations = 0;
    unsigned  releases    = 0;
    unsigned  uploads     = 0;
    glm::mat4 lastView{1.0f};
    glm::mat4 lastProjection{1.0f};
};

void requireCameraMatrixNear(const glm::mat4& actual, const glm::mat4& expected);
