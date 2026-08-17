#pragma once

#include "CGroup.h"
#include "Dimens.h"

#include <array>
#include <cstdint>
#include <vector>

class CAssembled : public CGroup {
public:
    using TransformMatrix = std::array<double, 16>;

    explicit CAssembled(std::string name = "Assembly");
    CAssembled(std::string name, std::vector<unsigned long> element_ids);
    ~CAssembled() override;

    CAssembled(const CAssembled&) = delete;
    CAssembled& operator=(const CAssembled&) = delete;

    const std::vector<CDimens3D*>& GetDimensions() const;
    void AddDimension(const CDimens3D& dimension);
    void ClearDimensions();

    std::uint8_t GetDrawParam() const;
    void SetDrawParam(std::uint8_t draw_param);
    unsigned long GetIdDim() const;
    void SetIdDim(unsigned long id);

    const TransformMatrix& GetAssemblyTransform() const;
    void SetAssemblyTransform(const TransformMatrix& transform);
    bool HasAssemblyTransform() const;
    void ResetAssemblyTransform();
    void ApplyStoredTransformToElements() const;

    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    void Mirror(Vec3 plane_point, Vec3 plane_normal) override;
    bool CommitTranslate(Vec3 delta) override;
    bool CommitRotate(Vec3 center, Vec3 axis, float angle) override;
    bool CommitScale(Vec3 center, Vec3 axis, float factor) override;

    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;

protected:
    // CTechnology* m_Technology; // manufacturing technology (future)
    std::vector<CDimens3D*> m_dimens;
    std::uint8_t m_DrawParam = 0;
    unsigned long m_IdDim = 0;
    TransformMatrix m_AssemblyTransform{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0};
};
