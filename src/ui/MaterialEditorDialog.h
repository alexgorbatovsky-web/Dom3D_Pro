#pragma once

#include "../MaterialLibrary.h"

#include <QDialog>

#include <vector>

class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class MaterialSphereBrowser;

class MaterialEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit MaterialEditorDialog(const QString& library_path,
                                  const std::vector<Material>& document_materials,
                                  bool has_selection,
                                  const Material* initial_material = nullptr,
                                  QWidget* parent = nullptr);
    void SetCurrentMaterial(const Material& material, const QString& file_path = {});

signals:
    void ApplyMaterialToSelected(const Material& material);
    void SaveMaterialToDocument(const Material& material);
    void LibraryMaterialSaved(const QString& file_path);
    void RequestSelectedObjectMaterial();
    void RequestPaintMaterial(const Material& material);
    void RequestPickObjectMaterial();

private:
    void LoadLibrary();
    void PopulateTree();
    void PopulateDocumentMaterials();
    void SelectDocumentMaterialById(unsigned long id);
    void LoadMaterialToEditor(const Material& material, const QString& file_path);
    Material EditorMaterial() const;
    void SetColorButton(QPushButton* button, Color color);
    Color ButtonColor(QPushButton* button) const;
    void PickColor(QPushButton* button);
    void BrowseTexture(QLineEdit* edit);
    void CreateNewMaterial();
    void ExportCurrentMaterial();
    void ImportMaterialFromFile();
    void SaveCurrentMaterial();
    void ApplyCurrentMaterial();
    void CommitEditorChanges();
    void UpdateCoatingControls(const Material& material,
                               const QString& source_path);
    void ApplyCoatingControls();

    QString library_path_;
    QString current_file_path_;
    MaterialLibrary library_;
    std::vector<Material> document_materials_;
    Material initial_material_;
    bool has_initial_material_ = false;
    bool loading_editor_ = false;
    unsigned long selected_document_material_id_ = 0;

    QListWidget* document_materials_list_ = nullptr;
    QTreeWidget* material_tree_ = nullptr;
    MaterialSphereBrowser* material_browser_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QLineEdit* id_edit_ = nullptr;
    QPushButton* ambient_button_ = nullptr;
    QPushButton* diffuse_button_ = nullptr;
    QPushButton* emission_button_ = nullptr;
    QGroupBox* coating_group_ = nullptr;
    QComboBox* ral_combo_ = nullptr;
    QCheckBox* lacquered_check_ = nullptr;
    QLabel* film_type_label_ = nullptr;
    QComboBox* film_type_combo_ = nullptr;
    int coating_family_ = 0;
    QDoubleSpinBox* alpha_spin_ = nullptr;
    QDoubleSpinBox* specular_spin_ = nullptr;
    QDoubleSpinBox* shininess_spin_ = nullptr;
    QDoubleSpinBox* reflectivity_spin_ = nullptr;
    QDoubleSpinBox* roughness_spin_ = nullptr;
    QDoubleSpinBox* metallic_spin_ = nullptr;
    QDoubleSpinBox* coat_weight_spin_ = nullptr;
    QDoubleSpinBox* coat_roughness_spin_ = nullptr;
    QDoubleSpinBox* normal_strength_spin_ = nullptr;
    QDoubleSpinBox* displacement_scale_spin_ = nullptr;
    QDoubleSpinBox* texture_offset_u_spin_ = nullptr;
    QDoubleSpinBox* texture_offset_v_spin_ = nullptr;
    QDoubleSpinBox* texture_scale_u_spin_ = nullptr;
    QDoubleSpinBox* texture_scale_v_spin_ = nullptr;
    QDoubleSpinBox* texture_rotation_spin_ = nullptr;
    QCheckBox* texture_rotate_90_check_ = nullptr;
    QCheckBox* texture_fit_to_surface_check_ = nullptr;
    QLineEdit* color_texture_edit_ = nullptr;
    QLineEdit* light_texture_edit_ = nullptr;
    QLineEdit* bump_texture_edit_ = nullptr;
    QLineEdit* normal_texture_edit_ = nullptr;
    QLineEdit* roughness_texture_edit_ = nullptr;
    QLineEdit* metallic_texture_edit_ = nullptr;
    QLineEdit* displacement_texture_edit_ = nullptr;
    QPushButton* apply_button_ = nullptr;
};
