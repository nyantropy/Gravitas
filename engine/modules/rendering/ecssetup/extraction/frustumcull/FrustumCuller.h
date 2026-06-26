#pragma once
#include "GlmConfig.h"
#include <array>

#include "RenderExtractionSnapshot.h"

// Extracts the 6 view-frustum planes from a view-projection matrix and tests
// AABBs against them.  No ECS dependency — takes plain GLM types only.
class FrustumCuller
{
public:
    // Extract the 6 frustum planes from a combined view-projection matrix.
    // Uses the Gribb-Hartmann method: each plane is a row combination of the
    // matrix.  Planes are NOT normalised — isVisible uses the signed-distance
    // form which does not require unit normals for a correct inside/outside test.
    static FrustumPlanes extractPlanesFromMatrix(const glm::mat4& viewProjection)
    {
        const glm::mat4& m = viewProjection;
        FrustumPlanes extracted{};

        // Each plane is stored as (normal.x, normal.y, normal.z, distance).
        // Row indices follow GLM column-major convention: m[col][row].

        // Left   : col3 + col0
        extracted[0] = glm::vec4(
            m[0][3] + m[0][0],
            m[1][3] + m[1][0],
            m[2][3] + m[2][0],
            m[3][3] + m[3][0]);

        // Right  : col3 - col0
        extracted[1] = glm::vec4(
            m[0][3] - m[0][0],
            m[1][3] - m[1][0],
            m[2][3] - m[2][0],
            m[3][3] - m[3][0]);

        // Bottom : col3 + col1
        extracted[2] = glm::vec4(
            m[0][3] + m[0][1],
            m[1][3] + m[1][1],
            m[2][3] + m[2][1],
            m[3][3] + m[3][1]);

        // Top    : col3 - col1
        extracted[3] = glm::vec4(
            m[0][3] - m[0][1],
            m[1][3] - m[1][1],
            m[2][3] - m[2][1],
            m[3][3] - m[3][1]);

        // Near   : col3 + col2
        extracted[4] = glm::vec4(
            m[0][3] + m[0][2],
            m[1][3] + m[1][2],
            m[2][3] + m[2][2],
            m[3][3] + m[3][2]);

        // Far    : col3 - col2
        extracted[5] = glm::vec4(
            m[0][3] - m[0][2],
            m[1][3] - m[1][2],
            m[2][3] - m[2][2],
            m[3][3] - m[3][2]);

        return extracted;
    }

    void extractPlanes(const glm::mat4& viewProjection)
    {
        planes = extractPlanesFromMatrix(viewProjection);
    }

    // Test a world-space AABB against the frustum.
    // Returns true if the AABB is fully or partially inside the frustum.
    // Returns false only if the AABB is fully outside at least one plane.
    //
    // The positive-vertex test is used: for each plane, find the corner of
    // the AABB that is furthest in the direction of the plane normal (the
    // "positive vertex").  If that corner is on the outside half-space of
    // any plane, the entire AABB is outside the frustum.
    //
    // This is conservative: AABBs that straddle a frustum edge are treated
    // as visible.  This is always correct — it never culls something that
    // should be visible.
    static bool isVisible(const FrustumPlanes& frustum,
                          const glm::vec3& worldMin,
                          const glm::vec3& worldMax)
    {
        for (const glm::vec4& plane : frustum)
        {
            // Positive vertex: pick the corner maximally aligned with the plane normal.
            glm::vec3 positive{
                plane.x >= 0.0f ? worldMax.x : worldMin.x,
                plane.y >= 0.0f ? worldMax.y : worldMin.y,
                plane.z >= 0.0f ? worldMax.z : worldMin.z
            };

            // If the positive vertex is outside this plane, the AABB is culled.
            if (plane.x * positive.x +
                plane.y * positive.y +
                plane.z * positive.z +
                plane.w < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    bool isVisible(const glm::vec3& worldMin, const glm::vec3& worldMax) const
    {
        return isVisible(planes, worldMin, worldMax);
    }

    const FrustumPlanes& getPlanes() const
    {
        return planes;
    }

    void setPlanes(const FrustumPlanes& newPlanes)
    {
        planes = newPlanes;
    }

private:
    FrustumPlanes planes{};
};
