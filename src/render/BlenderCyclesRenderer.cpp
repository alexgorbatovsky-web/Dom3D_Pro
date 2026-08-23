#include "BlenderCyclesRenderer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QRegularExpression>

namespace {
QString quoted(QString value) {
    return '"' + value.replace('"', "\\\"") + '"';
}
}

BlenderCyclesRenderer::BlenderCyclesRenderer(QString blender_path,
                                             QObject* parent)
    : QObject(parent), blender_path_(std::move(blender_path)) {
    process_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString text = QString::fromUtf8(process_.readAllStandardOutput());
        process_output_ += text;
        emit OutputReceived(text);
        HandleOutput(text);
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &BlenderCyclesRenderer::FinishProcess);
}

BlenderCyclesRenderer::~BlenderCyclesRenderer() {
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(3000);
    }
}

QString BlenderCyclesRenderer::Name() const { return "Blender Cycles"; }
QString BlenderCyclesRenderer::BlenderPath() const { return blender_path_; }
QString BlenderCyclesRenderer::DiagnosticDirectory() const {
    return temporary_directory_ ? temporary_directory_->path() : QString{};
}

QString BlenderCyclesRenderer::FindBlender() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    const QString saved = settings.value("render/blenderPath").toString();
    if (QFileInfo::exists(saved)) return QFileInfo(saved).absoluteFilePath();
    QStringList candidates;
    const QString program_files = qEnvironmentVariable("ProgramFiles");
    const QString local_app_data = qEnvironmentVariable("LOCALAPPDATA");
    for (const QString& root : {
             QDir(program_files).filePath("Blender Foundation"),
             QDir(local_app_data).filePath("Programs/Blender Foundation")}) {
        QDir directory(root);
        if (!directory.exists()) continue;
        const QStringList versions = directory.entryList(
            QStringList{"Blender *"}, QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::Reversed);
        for (const QString& version : versions) {
            const QString executable = directory.filePath(version + "/blender.exe");
            if (QFileInfo::exists(executable)) candidates.push_back(executable);
        }
    }
    return candidates.isEmpty() ? QString{} : QFileInfo(candidates.front()).absoluteFilePath();
}

bool BlenderCyclesRenderer::IsAvailable(QString* error) const {
    if (blender_path_.isEmpty() || !QFileInfo::exists(blender_path_)) {
        if (error) *error = "Blender executable was not found.";
        return false;
    }
    QProcess check;
    check.start(blender_path_, {"--version"});
    if (!check.waitForStarted(5000) || !check.waitForFinished(10000)
        || check.exitStatus() != QProcess::NormalExit || check.exitCode() != 0) {
        if (error) *error = "The selected Blender executable could not be started.";
        return false;
    }
    return true;
}

bool BlenderCyclesRenderer::StartRender(const RenderScene& scene,
                                        const RenderSettings& settings,
                                        QString* error) {
    if (process_.state() != QProcess::NotRunning) {
        if (error) *error = "A Blender render is already running.";
        return false;
    }
    if (!IsAvailable(error)) return false;
    if (scene.meshes.empty()) {
        if (error) *error = "The current scene has no visible renderable geometry.";
        return false;
    }
    if (settings.output_file.isEmpty()) {
        if (error) *error = "Choose an output PNG file.";
        return false;
    }

    temporary_directory_ = std::make_unique<QTemporaryDir>(
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath("Dom3D_Cycles_XXXXXX"));
    if (!temporary_directory_->isValid()) {
        if (error) *error = "Could not create a temporary render directory.";
        return false;
    }
    scene_ = scene;
    settings_ = settings;
    process_output_.clear();
    output_line_buffer_.clear();
    cancel_requested_ = false;
    final_phase_ = false;
    const QString json_path = temporary_directory_->filePath("scene.json");
    const QString script_path = temporary_directory_->filePath("dom3d_cycles.py");
    if (!scene_.SaveJson(json_path, settings_, error)) return false;
    QFile script(script_path);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = script.errorString();
        return false;
    }
    script.write(PythonScript().toUtf8());
    script.close();

    const QStringList arguments{
        "--background", "--factory-startup", "--python", script_path,
        "--", json_path};
    command_line_ = quoted(blender_path_);
    for (const QString& argument : arguments) command_line_ += ' ' + quoted(argument);
    process_.setWorkingDirectory(temporary_directory_->path());
    timer_.start();
    process_.start(blender_path_, arguments);
    if (!process_.waitForStarted(5000)) {
        if (error) *error = process_.errorString();
        temporary_directory_->setAutoRemove(false);
        WriteLog(*error);
        return false;
    }
    QSettings("Dom3D", "Dom3D_Pro").setValue("render/blenderPath", blender_path_);
    emit RenderStarted();
    return true;
}

void BlenderCyclesRenderer::Cancel() {
    if (process_.state() == QProcess::NotRunning) return;
    cancel_requested_ = true;
    process_.terminate();
    if (!process_.waitForFinished(2500)) process_.kill();
}

void BlenderCyclesRenderer::FinishProcess(int exit_code,
                                          QProcess::ExitStatus status) {
    const QString tail = QString::fromUtf8(process_.readAllStandardOutput());
    process_output_ += tail;
    HandleOutput(tail + "\n");
    if (cancel_requested_) {
        WriteLog("Canceled by user.");
        emit RenderCanceled();
        return;
    }
    if (status == QProcess::NormalExit && exit_code == 0
        && QFileInfo::exists(settings_.output_file)) {
        WriteLog();
        emit RenderProgress(100, "Finished");
        emit RenderFinished(settings_.output_file, timer_.elapsed() / 1000.0);
        return;
    }
    const QString message = QString(
        "Blender Cycles failed (exit code %1).\n%2")
        .arg(exit_code)
        .arg(process_output_.right(4000));
    temporary_directory_->setAutoRemove(false);
    WriteLog(message);
    emit RenderFailed(message, temporary_directory_->path());
}

void BlenderCyclesRenderer::HandleOutput(const QString& text) {
    output_line_buffer_ += text;
    output_line_buffer_.replace('\r', '\n');
    int newline = -1;
    while ((newline = output_line_buffer_.indexOf('\n')) >= 0) {
        const QString line = output_line_buffer_.left(newline).trimmed();
        output_line_buffer_.remove(0, newline + 1);
        if (!line.isEmpty()) HandleOutputLine(line);
    }
}

void BlenderCyclesRenderer::HandleOutputLine(const QString& line) {
    if (line.startsWith("DOM3D_PHASE:FINAL")) {
        final_phase_ = true;
        emit RenderProgress(12, "Final render");
        return;
    }
    if (line.startsWith("DOM3D_PHASE:PREVIEW")) {
        final_phase_ = false;
        emit RenderProgress(2, "Preparing preview");
        return;
    }
    if (line.startsWith("DOM3D_PREVIEW:")) {
        const QString path = line.mid(QString("DOM3D_PREVIEW:").size()).trimmed();
        if (QFileInfo::exists(path)) emit PreviewUpdated(path);
        emit RenderProgress(10, "Preview ready");
        return;
    }

    // Blender/Cycles reports actual rendered samples on stdout. Support the
    // formats used by current and older Blender releases.
    static const QRegularExpression samples_re(
        R"((?:Rendering\s+(\d+)\s*/\s*(\d+)\s+samples?|Sample\s+(\d+)\s*/\s*(\d+)))",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = samples_re.match(line);
    if (!match.hasMatch()) return;
    const int sample = (match.captured(1).isEmpty()
        ? match.captured(3) : match.captured(1)).toInt();
    const int total = (match.captured(2).isEmpty()
        ? match.captured(4) : match.captured(2)).toInt();
    if (total <= 0 || sample < 0 || sample > total) return;
    const int local = sample * 100 / total;
    if (final_phase_) {
        emit RenderProgress(12 + local * 86 / 100,
                            QString("Final render — sample %1/%2")
                                .arg(sample).arg(total));
    } else {
        emit RenderProgress(2 + local * 7 / 100,
                            QString("Preview — sample %1/%2")
                                .arg(sample).arg(total));
    }
}

void BlenderCyclesRenderer::WriteLog(const QString& final_error) {
    if (!temporary_directory_) return;
    QFile log(temporary_directory_->filePath("render.log"));
    if (!log.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream stream(&log);
    stream << "Renderer: Blender Cycles\n";
    stream << "Blender: " << blender_path_ << "\n";
    stream << "Command: " << command_line_ << "\n";
    stream << "Objects: " << scene_.meshes.size() << "\n";
    stream << "Triangles: " << scene_.TriangleCount() << "\n";
    stream << "Materials: " << scene_.materials.size() << "\n";
    stream << "HDRI: " << scene_.environment.hdri_path << "\n";
    stream << "Device: "
           << (settings_.device == RenderSettings::Device::CPU ? "CPU"
               : settings_.device == RenderSettings::Device::GPU ? "GPU" : "AUTO")
           << "\n";
    stream << "Elapsed: " << timer_.elapsed() / 1000.0 << " s\n";
    if (!final_error.isEmpty()) stream << "Error: " << final_error << "\n";
    stream << "\n--- Blender output ---\n" << process_output_;
}

QString BlenderCyclesRenderer::PythonScript() {
    return QString::fromUtf8(R"PY(
import bpy, json, math, os, sys, time, traceback
from mathutils import Matrix, Vector

def fail(message):
    print("DOM3D_ERROR:", message, file=sys.stderr, flush=True)
    raise RuntimeError(message)

def socket(node, names):
    for name in names:
        value = node.inputs.get(name)
        if value is not None:
            return value
    return None

def set_socket(node, names, value):
    target = socket(node, names)
    if target is not None:
        target.default_value = value
        return True
    return False

def load_image(path, non_color=False):
    if not path or not os.path.isfile(path):
        if path: print("DOM3D_WARNING: missing texture", path, flush=True)
        return None
    image = bpy.data.images.load(path, check_existing=True)
    if non_color:
        try: image.colorspace_settings.name = 'Non-Color'
        except Exception: pass
    return image

def image_node(nodes, path, non_color=False):
    image = load_image(path, non_color)
    if image is None: return None
    node = nodes.new('ShaderNodeTexImage')
    node.image = image
    node.extension = 'REPEAT'
    return node

def create_material(data, index):
    material = bpy.data.materials.new(data.get('name') or ('Material_%d' % index))
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    color = data.get('base_color', [0.8, 0.8, 0.8])
    alpha = min(1.0, max(0.0, data.get('alpha', 1.0)))
    legacy_reflection = min(1.0, max(0.0, data.get('reflectivity', 0.0)))
    legacy_specular = min(1.0, max(0.0, data.get('specular', 0.2)))
    legacy_shininess = max(1.0, data.get('shininess', 24.0))
    pbr_metallic = min(1.0, max(0.0, data.get('metallic', 0.0)))
    pbr_roughness = min(1.0, max(0.0, data.get('roughness', 0.5)))

    # Old Dom materials express mirror strength and gloss independently from
    # diffuse colour. Principled BSDF does not, so translate strong legacy
    # reflection into a neutral metal component and derive GGX roughness from
    # the old Blinn/Phong exponent. Low reflection remains a clear coat, which
    # keeps lacquered wood dielectric instead of turning it into copper.
    legacy_metallic = min(1.0, max(0.0, (legacy_reflection - 0.25) / 0.75))
    metallic_value = max(pbr_metallic, legacy_metallic)
    if legacy_metallic > pbr_metallic:
        neutral = 0.72 + 0.22 * legacy_reflection
        blend = legacy_metallic - pbr_metallic
        color = [value * (1.0 - blend) + neutral * blend for value in color]
    legacy_roughness = min(0.65, max(0.025,
        math.sqrt(2.0 / (legacy_shininess + 2.0))))
    roughness_value = pbr_roughness
    if legacy_reflection > 0.01 or legacy_specular > 0.35:
        roughness_value = min(roughness_value, legacy_roughness)

    transmission = 1.0 - alpha
    named_glass = 'glass' in material.name.casefold()
    if named_glass: transmission = 1.0
    set_socket(bsdf, ['Base Color'], (*color, 1.0))
    set_socket(bsdf, ['Metallic'], 0.0 if transmission > 0.001 else metallic_value)
    set_socket(bsdf, ['Roughness'], roughness_value)
    set_socket(bsdf, ['Alpha'], 1.0)
    set_socket(bsdf, ['Transmission Weight', 'Transmission'], transmission)
    set_socket(bsdf, ['IOR'], 1.45)
    set_socket(bsdf, ['Specular IOR Level', 'Specular'],
               max(0.5, legacy_specular, legacy_reflection))
    emission = data.get('emission', [0.0, 0.0, 0.0])
    set_socket(bsdf, ['Emission Color', 'Emission'], (*emission, 1.0))
    set_socket(bsdf, ['Emission Strength'], 1.0 if max(emission) > 0.0 else 0.0)
    set_socket(bsdf, ['Coat Weight', 'Clearcoat'], max(
        data.get('coat_weight', 0.0), min(1.0, legacy_reflection * 1.5)))
    set_socket(bsdf, ['Coat Roughness', 'Clearcoat Roughness'], data.get('coat_roughness', 0.05))
    set_socket(bsdf, ['Coat IOR'], 1.5)
    if named_glass or transmission > 0.001:
        set_socket(bsdf, ['Transmission Weight', 'Transmission'],
                   1.0 if named_glass else transmission)
        set_socket(bsdf, ['IOR'], 1.45)
        set_socket(bsdf, ['Roughness'], max(0.015, min(0.08, roughness_value)))
        set_socket(bsdf, ['Alpha'], 1.0)

    base = image_node(nodes, data.get('color_texture', ''), False)
    if base:
        multiply = nodes.new('ShaderNodeMixRGB')
        multiply.blend_type = 'MULTIPLY'
        multiply.inputs[0].default_value = 1.0
        multiply.inputs[2].default_value = (*color, 1.0)
        links.new(base.outputs['Color'], multiply.inputs[1])
        links.new(multiply.outputs['Color'], socket(bsdf, ['Base Color']))
    roughness = image_node(nodes, data.get('roughness_texture', ''), True)
    if roughness: links.new(roughness.outputs['Color'], socket(bsdf, ['Roughness']))
    metallic = image_node(nodes, data.get('metallic_texture', ''), True)
    if metallic: links.new(metallic.outputs['Color'], socket(bsdf, ['Metallic']))

    normal_input = socket(bsdf, ['Normal'])
    normal = image_node(nodes, data.get('normal_texture', ''), True)
    normal_map = None
    if normal and normal_input:
        normal_map = nodes.new('ShaderNodeNormalMap')
        normal_map.inputs['Strength'].default_value = data.get('normal_strength', 1.0)
        links.new(normal.outputs['Color'], normal_map.inputs['Color'])
        links.new(normal_map.outputs['Normal'], normal_input)
    height_path = data.get('bump_texture', '') or data.get('displacement_texture', '')
    height = image_node(nodes, height_path, True)
    if height and normal_input:
        bump = nodes.new('ShaderNodeBump')
        bump.inputs['Strength'].default_value = max(0.0, data.get('bump_strength', 0.03))
        links.new(height.outputs['Color'], bump.inputs['Height'])
        if normal_map: links.new(normal_map.outputs['Normal'], bump.inputs['Normal'])
        links.new(bump.outputs['Normal'], normal_input)
    return material

def configure_device(scene, requested):
    selected = 'CPU'
    if requested != 'CPU':
        try:
            prefs = bpy.context.preferences.addons['cycles'].preferences
            for backend in ('OPTIX', 'CUDA', 'HIP', 'ONEAPI', 'METAL'):
                try:
                    prefs.compute_device_type = backend
                    prefs.get_devices()
                    devices = [d for d in prefs.devices if d.type != 'CPU']
                    if devices:
                        for device in prefs.devices: device.use = (device.type != 'CPU')
                        scene.cycles.device = 'GPU'
                        selected = 'GPU/' + backend
                        break
                except Exception: pass
        except Exception as exc:
            print('DOM3D_WARNING: GPU setup failed:', exc, flush=True)
    if requested == 'GPU' and selected == 'CPU':
        print('DOM3D_WARNING: GPU unavailable, falling back to CPU', flush=True)
    print('DOM3D_DEVICE:', selected, flush=True)

)PY") + QString::fromUtf8(R"PY(
def build_world(scene, data, settings):
    world = bpy.data.worlds.new('Dom3D World')
    world.use_nodes = True
    scene.world = world
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputWorld')
    background = nodes.new('ShaderNodeBackground')
    reflection_background = nodes.new('ShaderNodeBackground')
    environment_strength = settings.get('environment_strength', 1.0)
    source_strength = data.get('strength', 1.0)
    background.inputs['Strength'].default_value = source_strength * environment_strength
    reflection_background.inputs['Strength'].default_value = source_strength * max(1.0, environment_strength)
    background_color = (*data.get('background_color', [0.055,0.065,0.08]), 1.0)
    background.inputs['Color'].default_value = background_color
    reflection_background.inputs['Color'].default_value = background_color
    light_path = nodes.new('ShaderNodeLightPath')
    reflection_ray = nodes.new('ShaderNodeMath')
    reflection_ray.operation = 'MAXIMUM'
    links.new(light_path.outputs['Is Glossy Ray'], reflection_ray.inputs[0])
    links.new(light_path.outputs['Is Transmission Ray'], reflection_ray.inputs[1])
    world_mix = nodes.new('ShaderNodeMixShader')
    links.new(reflection_ray.outputs[0], world_mix.inputs[0])
    links.new(background.outputs['Background'], world_mix.inputs[1])
    links.new(reflection_background.outputs['Background'], world_mix.inputs[2])
    links.new(world_mix.outputs['Shader'], output.inputs['Surface'])
    path = data.get('hdri_path', '')
    if data.get('enabled', False) and path and os.path.isfile(path):
        environment = nodes.new('ShaderNodeTexEnvironment')
        environment.image = load_image(path, False)
        texcoord = nodes.new('ShaderNodeTexCoord')
        # Dom3D samples its panorama in eye space: X=screen right, Y=screen
        # up and Z=eye depth. Convert the Blender world ray into CAMERA space,
        # then remap it to Blender's equirectangular convention (Z is up).
        view_transform = nodes.new('ShaderNodeVectorTransform')
        view_transform.vector_type = 'VECTOR'
        view_transform.convert_from = 'WORLD'
        view_transform.convert_to = 'CAMERA'
        axis_rotate = nodes.new('ShaderNodeVectorRotate')
        axis_rotate.rotation_type = 'X_AXIS'
        axis_rotate.inputs['Angle'].default_value = math.radians(90.0)
        handedness = nodes.new('ShaderNodeVectorMath')
        handedness.operation = 'MULTIPLY'
        # Blender's equirectangular vertical axis is opposite to the panorama
        # convention used by the modeling viewport. Flip Z as well as the
        # camera-space handedness so floor and ceiling stay upright.
        handedness.inputs[1].default_value = (1.0, -1.0, -1.0)
        hdri_rotate = nodes.new('ShaderNodeVectorRotate')
        hdri_rotate.rotation_type = 'Z_AXIS'
        hdri_rotate.inputs['Angle'].default_value = math.radians(
            data.get('rotation_degrees', 0.0))
        links.new(texcoord.outputs['Normal'], view_transform.inputs['Vector'])
        links.new(view_transform.outputs['Vector'], axis_rotate.inputs['Vector'])
        links.new(axis_rotate.outputs['Vector'], handedness.inputs[0])
        links.new(handedness.outputs['Vector'], hdri_rotate.inputs['Vector'])
        links.new(hdri_rotate.outputs['Vector'], environment.inputs['Vector'])
        links.new(environment.outputs['Color'], background.inputs['Color'])
        links.new(environment.outputs['Color'], reflection_background.inputs['Color'])
    elif path:
        print('DOM3D_WARNING: missing HDRI', path, flush=True)
)PY") + QString::fromUtf8(R"PY(
def add_custom_lights(scene, lights, scale):
    for index, item in enumerate(lights):
        if not item.get('enabled', True): continue
        kind = item.get('type', 'OMNI')
        blender_kind = 'SPOT' if kind == 'SPOT' else 'SUN' if kind == 'DIRECTIONAL' else 'POINT'
        light_data = bpy.data.lights.new('Dom3D Custom Light-%d' % (index + 1), blender_kind)
        diffuse = item.get('diffuse', [1.0, 1.0, 1.0])
        specular = item.get('specular', [1.0, 1.0, 1.0])
        strength = max(max(diffuse), 0.0)
        light_data.color = tuple(value / strength for value in diffuse) if strength > 0.0 else (1.0, 1.0, 1.0)
        # Legacy Dom3D colors also carry light intensity. Default custom
        # colors are normalized to the Auto rig, so 1000 W preserves the
        # same total output while still allowing direct RGB intensity edits.
        light_data.energy = (3.0 if blender_kind == 'SUN' else 1000.0) * strength
        if hasattr(light_data, 'diffuse_factor'): light_data.diffuse_factor = 1.0
        if hasattr(light_data, 'specular_factor'):
            light_data.specular_factor = max(specular) / max(strength, 0.0001)
        size = max(0.0, item.get('size', 0.0)) * scale
        if blender_kind == 'SUN':
            light_data.angle = min(math.radians(30.0), size)
        else:
            light_data.shadow_soft_size = size
        if blender_kind == 'SPOT':
            falloff = max(item.get('falloff', 0.9), 0.01)
            hotspot = min(max(item.get('hotspot', 0.65), 0.0), falloff)
            light_data.spot_size = min(math.pi, falloff * 2.0)
            exponent = max(0.0, item.get('spot_exponent', 0.0))
            light_data.spot_blend = (min(1.0, max(0.0, 1.0 - hotspot / falloff))
                                     / (1.0 + exponent / 16.0))
        if item.get('attenuation_enabled', False) and item.get('range', 0.0) > 0.0:
            light_data.use_custom_distance = True
            light_data.cutoff_distance = item['range'] * scale
        casts_shadows = item.get('casts_shadows', True)
        shadow_density = min(1.0, max(0.0, item.get('shadow_density', 1.0)))
        light_data.use_shadow = casts_shadows and shadow_density > 0.0
        full_energy = light_data.energy
        if casts_shadows and shadow_density < 1.0:
            light_data.energy = full_energy * shadow_density
        light_object = bpy.data.objects.new(light_data.name, light_data)
        scene.collection.objects.link(light_object)
        position = item.get('position', [0.0, 0.0, 0.0])
        light_object.location = Vector(tuple(value * scale for value in position))
        direction = Vector(item.get('direction', [0.0, 0.0, -1.0]))
        if direction.length < 0.000001: direction = Vector((0.0, 0.0, -1.0))
        light_object.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
        if casts_shadows and shadow_density < 1.0:
            fill_data = light_data.copy()
            fill_data.name = light_data.name + ' Shadow Fill'
            fill_data.energy = full_energy * (1.0 - shadow_density)
            fill_data.use_shadow = False
            fill_object = bpy.data.objects.new(fill_data.name, fill_data)
            scene.collection.objects.link(fill_object)
            fill_object.matrix_world = light_object.matrix_world.copy()

def add_render_lights(scene, camera_object, environment, settings,
                      custom_lights, scale):
    if settings.get('light_mode', 'AUTO') == 'CUSTOMIZE':
        add_custom_lights(scene, custom_lights, scale)
        return
    path = environment.get('hdri_path', '')
    has_environment = (environment.get('enabled', False)
                       and path and os.path.isfile(path))
    mesh_objects = [obj for obj in scene.objects if obj.type == 'MESH']
    if not mesh_objects:
        return
    room_objects = [obj for obj in mesh_objects
                    if obj.name.casefold().startswith('room ')]
    bounds_objects = room_objects or mesh_objects
    corners = [obj.matrix_world @ Vector(corner)
               for obj in bounds_objects for corner in obj.bound_box]
    minimum = Vector((min(p.x for p in corners), min(p.y for p in corners),
                      min(p.z for p in corners)))
    maximum = Vector((max(p.x for p in corners), max(p.y for p in corners),
                      max(p.z for p in corners)))
    center = (minimum + maximum) * 0.5
    size_x = max(maximum.x - minimum.x, 0.25)
    size_y = max(maximum.y - minimum.y, 0.25)
    size_z = max(maximum.z - minimum.z, 0.25)
    extent = max(size_x, size_y, size_z)
    orientation = camera_object.matrix_world.to_quaternion()
    right = orientation @ Vector((1.0, 0.0, 0.0))
    up = orientation @ Vector((0.0, 1.0, 0.0))
    forward = orientation @ Vector((0.0, 0.0, -1.0))

    def add_area(name, location, energy, size, color, target=center,
                 reflection_card=False):
        light_data = bpy.data.lights.new(name, type='AREA')
        light_data.energy = energy
        light_data.shape = 'DISK'
        light_data.size = size
        light_data.color = color
        light_object = bpy.data.objects.new(name, light_data)
        scene.collection.objects.link(light_object)
        light_object.location = location
        light_object.rotation_euler = (target - location).to_track_quat('-Z', 'Y').to_euler()
        # Room lights stay out of reflections. Object-only scenes deliberately
        # keep broad softboxes visible to glossy rays: polished metal needs
        # large bright shapes to reflect, just like a real product studio.
        for visibility in ('visible_camera', 'visible_transmission'):
            if hasattr(light_object, visibility):
                setattr(light_object, visibility, False)
        if hasattr(light_object, 'visible_glossy'):
            light_object.visible_glossy = reflection_card

    preset = settings.get('lighting_preset', 'INTERIOR')
    light_strength = settings.get('interior_light_strength', 1.0)
    power = 850.0 * max(extent * extent, 0.25)
    standalone_reflections = not room_objects
    if standalone_reflections or preset == 'STUDIO' or (preset == 'EXTERIOR' and not has_environment):
        add_area('Dom3D Key Softbox',
                 center - right * extent * 0.75 + up * extent * 0.85
                        - forward * extent * 1.15,
                 power, extent * 0.9, (1.0, 0.94, 0.88),
                 reflection_card=standalone_reflections)
        add_area('Dom3D Fill Softbox',
                 center + right * extent * 0.95 + up * extent * 0.25
                        - forward * extent * 0.75,
                 power * 0.38, extent * 1.15, (0.82, 0.90, 1.0),
                 reflection_card=standalone_reflections)
        if standalone_reflections:
            add_area('Dom3D Rim Softbox',
                     center + forward * extent * 0.70 + up * extent * 0.55,
                     power * 0.28, extent * 0.75, (1.0, 1.0, 1.0),
                     reflection_card=True)
    if standalone_reflections:
        return
    if preset != 'INTERIOR' or light_strength <= 0.0:
        return

    # Three adaptive area sources reproduce the classic Dom3D Light-1/2/3
    # setup without hard-coded coordinates. They remain inside the room so
    # walls cannot block the fill, and broad disks keep shadows soft.
    floor_area = size_x * size_y
    total_ceiling_power = min(max(floor_area * 55.0, 450.0), 2800.0) * light_strength
    ceiling_z = maximum.z - min(0.12, size_z * 0.04)
    target = Vector((center.x, center.y, minimum.z + size_z * 0.32))
    base_size = max(0.55, min(size_x, size_y) * 0.34)
    add_area('Dom3D Interior Light-1',
             Vector((center.x + size_x * 0.18,
                     center.y + size_y * 0.06, ceiling_z)),
             total_ceiling_power * 0.52, base_size,
             (1.0, 0.86, 0.72), target)
    add_area('Dom3D Interior Light-2',
             Vector((center.x - size_x * 0.28,
                     center.y - size_y * 0.24,
                     minimum.z + size_z * 0.76)),
             total_ceiling_power * 0.32, base_size * 1.35,
             (0.90, 0.95, 1.0), target)
    add_area('Dom3D Interior Light-3',
             Vector((center.x - size_x * 0.34,
                     center.y + size_y * 0.28,
                     minimum.z + size_z * 0.60)),
             total_ceiling_power * 0.16, base_size * 0.9,
             (0.70, 0.64, 0.56), target)

    # Do not place artificial lights directly in window openings. At that
    # distance an area light creates white hot spots around the frame and
    # enlarged wedge-shaped shadows in room corners. Daylight comes from the
    # world/HDRI; the broad ceiling source supplies the interior fill.

def main():
    marker = sys.argv.index('--')
    json_path = sys.argv[marker + 1]
    with open(json_path, 'r', encoding='utf-8') as handle:
        data = json.load(handle)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    settings = data['settings']
    scene.render.engine = 'CYCLES'
    scene.render.resolution_x = settings['width']
    scene.render.resolution_y = settings['height']
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.film_transparent = settings.get('transparent_background', False)
    scene.render.filepath = settings['output_file']
    scene.cycles.samples = settings['samples']
    scene.cycles.use_denoising = settings.get('denoise', True)
    if hasattr(scene.cycles, 'use_adaptive_sampling'):
        scene.cycles.use_adaptive_sampling = True
        scene.cycles.adaptive_threshold = settings.get('noise_threshold', 0.05)
    configure_device(scene, settings.get('device', 'AUTO'))
    view = scene.view_settings
    try: view.view_transform = settings.get('view_transform', 'AgX')
    except Exception: view.view_transform = 'Standard'
    try: view.look = settings.get('look', 'Medium High Contrast')
    except Exception:
        try: view.look = 'None'
        except Exception: pass
    view.exposure = settings.get('exposure', 0.0)
    view.gamma = 1.0

    materials = [create_material(item, i) for i, item in enumerate(data['materials'])]
    scale = data.get('unit_scale', 0.001)
    for item in data['meshes']:
        vertices = [[v[0]*scale, v[1]*scale, v[2]*scale] for v in item['vertices']]
        faces = [triangle['vertices'] for triangle in item['triangles']]
        mesh = bpy.data.meshes.new(item['name'])
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        uv_layer = mesh.uv_layers.new(name='UVMap')
        loop_normals = []
        # Architectural room surfaces are planar CAD faces. Smooth vertex
        # normals can turn tiny tessellation differences into broad diagonal
        # bands across ceilings and walls, so keep those faces strictly flat.
        flat_shaded = item['name'].casefold().startswith('room ')
        for polygon, triangle in zip(mesh.polygons, item['triangles']):
            polygon.use_smooth = not flat_shaded
            for loop_index, uv, normal in zip(polygon.loop_indices, triangle['uvs'], triangle['normals']):
                uv_layer.data[loop_index].uv = uv
                loop_normals.append(tuple(normal))
        if not flat_shaded:
            try: mesh.normals_split_custom_set(loop_normals)
            except Exception as exc: print('DOM3D_WARNING: custom normals:', exc, flush=True)
        obj = bpy.data.objects.new(item['name'], mesh)
        scene.collection.objects.link(obj)
        material_index = item.get('material', -1)
        if 0 <= material_index < len(materials):
            render_material = materials[material_index]
            if item.get('independent_surface_patch', False):
                render_material = render_material.copy()
                render_material.name += ' CAD Surface'
                # IGES SurfaceSet faces have independent parametric UV frames.
                # A tangent-space normal/bump map is valid on a coherent Solid
                # but rotates independently on every patch and can turn a
                # polished object black in Cycles. Preserve Color, Roughness
                # and Metalness; use the exported CAD vertex normals instead.
                principled = next((node for node in render_material.node_tree.nodes
                    if node.type == 'BSDF_PRINCIPLED'), None)
                normal_socket = socket(principled, ['Normal']) if principled else None
                if normal_socket:
                    for link in list(normal_socket.links):
                        render_material.node_tree.links.remove(link)
            obj.data.materials.append(render_material)

    camera_data = data['camera']
    camera_object_data = bpy.data.cameras.new('Dom3D Camera')
    camera_object = bpy.data.objects.new('Dom3D Camera', camera_object_data)
    scene.collection.objects.link(camera_object)
    position = [value * scale for value in camera_data['position']]
    right = camera_data['right']; up = camera_data['up']; forward = camera_data['forward']
    camera_object.matrix_world = Matrix((
        (right[0], up[0], -forward[0], position[0]),
        (right[1], up[1], -forward[1], position[1]),
        (right[2], up[2], -forward[2], position[2]),
        (0.0, 0.0, 0.0, 1.0)))
    # Dom3D defines both perspective FOV and orthographic scale vertically.
    # Blender's AUTO fit interprets ortho_scale as the frame width on a
    # landscape image, which zooms the Cycles result by the aspect ratio.
    camera_object_data.sensor_fit = 'VERTICAL'
    camera_object_data.clip_start = 0.025
    if camera_data.get('orthographic', False):
        camera_object_data.type = 'ORTHO'
        camera_object_data.ortho_scale = camera_data['orthographic_scale_mm'] * scale
    else:
        camera_object_data.type = 'PERSP'
        camera_object_data.angle = math.radians(camera_data.get('vertical_fov_degrees', 50.0))
    scene.camera = camera_object
    build_world(scene, data['environment'], settings)
    add_render_lights(scene, camera_object, data['environment'], settings,
                      data.get('lights', []), scale)
    print('DOM3D_SCENE: objects=%d triangles=%d materials=%d Blender=%s' % (
        len(data['meshes']), sum(len(m['triangles']) for m in data['meshes']),
        len(materials), bpy.app.version_string), flush=True)

    # Produce a deliberately small, low-sample frame first. It gives the
    # render monitor a useful image almost immediately while leaving every
    # final-render setting untouched.
    final_path = scene.render.filepath
    final_samples = scene.cycles.samples
    final_denoise = scene.cycles.use_denoising
    preview_path = os.path.join(os.path.dirname(json_path), 'preview.png')
    print('DOM3D_PHASE:PREVIEW', flush=True)
    scene.render.filepath = preview_path
    scene.render.resolution_percentage = 25
    scene.cycles.samples = min(8, max(2, final_samples))
    scene.cycles.use_denoising = False
    bpy.ops.render.render(write_still=True)
    print('DOM3D_PREVIEW:' + preview_path, flush=True)

    scene.render.filepath = final_path
    scene.render.resolution_percentage = 100
    scene.cycles.samples = final_samples
    scene.cycles.use_denoising = final_denoise
    print('DOM3D_PHASE:FINAL', flush=True)
    bpy.ops.render.render(write_still=True)
    print('DOM3D_DONE:', scene.render.filepath, flush=True)

try:
    main()
except Exception:
    traceback.print_exc()
    sys.exit(2)
)PY");
}
