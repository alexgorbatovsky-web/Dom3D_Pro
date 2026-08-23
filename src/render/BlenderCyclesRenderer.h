#pragma once

#include "IExternalRenderer.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>

#include <memory>

class QTemporaryDir;

class BlenderCyclesRenderer final : public QObject,
                                    public IExternalRenderer {
    Q_OBJECT
public:
    explicit BlenderCyclesRenderer(QString blender_path,
                                   QObject* parent = nullptr);
    ~BlenderCyclesRenderer() override;

    QString Name() const override;
    bool IsAvailable(QString* error = nullptr) const override;
    bool StartRender(const RenderScene& scene,
                     const RenderSettings& settings,
                     QString* error = nullptr) override;
    void Cancel() override;

    static QString FindBlender();
    static QString PythonScript();
    QString BlenderPath() const;
    QString DiagnosticDirectory() const;

signals:
    void OutputReceived(QString text);
    void RenderStarted();
    void RenderProgress(int percent, QString stage);
    void PreviewUpdated(QString preview_file);
    void RenderFinished(QString output_file, double seconds);
    void RenderFailed(QString error, QString diagnostic_directory);
    void RenderCanceled();

private:
    void FinishProcess(int exit_code, QProcess::ExitStatus status);
    void WriteLog(const QString& final_error = {});
    void HandleOutput(const QString& text);
    void HandleOutputLine(const QString& line);

    QString blender_path_;
    QProcess process_;
    std::unique_ptr<QTemporaryDir> temporary_directory_;
    RenderScene scene_;
    RenderSettings settings_;
    QString process_output_;
    QString command_line_;
    QString output_line_buffer_;
    QElapsedTimer timer_;
    bool cancel_requested_ = false;
    bool final_phase_ = false;
};
