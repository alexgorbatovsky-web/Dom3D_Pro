#include "ImageReliefDialog.h"
#include "LanguageManager.h"
#include "ReliefPreview.h"
#include <TopExp_Explorer.hxx>
#include <QEventLoop>
#include <QTimer>
#include <future>
#include <mutex>
#include <atomic>
#include "../solid/Solid.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QSettings>

ImageReliefDialog::ImageReliefDialog(const CSolid& solid,QWidget* parent,int surface_index):QDialog(parent) {
    setObjectName("ImageReliefDialog");setWindowTitle(DomTranslate("Image Relief"));
    auto* root=new QHBoxLayout(this);auto* controls=new QWidget(this);controls->setMaximumWidth(500);auto* layout=new QVBoxLayout(controls);root->addWidget(controls);
    auto* right=new QVBoxLayout;root->addLayout(right,1);auto* preview=new ReliefPreview(this);preview->setObjectName("reliefPreview");right->addWidget(preview,1);auto* summary=new QLabel(this);summary->setWordWrap(true);right->addWidget(summary);resize(1000,750);
    int face_index=-1;
    if(surface_index>=0&&surface_index<solid.GetNumSurfaces()) {
        auto* face=solid.GetSurfaceFace(surface_index);int index=0;
        for(TopExp_Explorer ex(solid.m_Shape,TopAbs_FACE);ex.More();ex.Next(),++index)
            if(face&&ex.Current().IsSame(face->m_Face)){face_index=index;break;}
    }
    auto* body=new QLabel(QString::fromStdString(solid.GetName())+" / "+DomTranslate("Face %1").arg(surface_index+1),this);layout->addWidget(body);
    const bool surface_mapping=image_relief::UsesSurfaceMapping(solid.m_Shape,face_index);
    auto* help=new QLabel(DomTranslate("Relief is applied only to the selected face. First inspect a coarse preview, then build the final mesh."),this);
    help->setWordWrap(true);layout->addWidget(help);
    auto* image_button=new QPushButton(DomTranslate("Open ornament image..."),this);
    image_button->setObjectName("reliefImageButton");layout->addWidget(image_button);
    auto* thumbnail=new QLabel(this);thumbnail->setAlignment(Qt::AlignCenter);thumbnail->setFixedHeight(75);layout->addWidget(thumbnail);
    auto image=std::make_shared<QImage>();
    auto* form=new QFormLayout;layout->addLayout(form);
    auto spin=[&](const char* label,const char* name,double low,double high,double value,int decimals=2) {
        auto* s=new QDoubleSpinBox(this);s->setObjectName(name);s->setDecimals(decimals);s->setRange(low,high);s->setValue(value);form->addRow(DomTranslate(label),s);return s;
    };
    auto* spacing=spin("Mesh spacing (mm)","reliefSpacing",.05,100,2);
    spacing->setToolTip(DomTranslate("Maximum triangle edge before relief. Smaller spacing creates a denser mesh. Limit: 2 million triangles."));
    auto* height=spin("Relief height (mm)","reliefHeight",0,100,2);
    auto* axis=new QComboBox(this);axis->setObjectName("reliefAxis");axis->addItems({"X","Y","Z"});axis->setCurrentIndex(2);form->addRow(DomTranslate("Vase axis"),axis);
    auto* ru=new QSpinBox(this);ru->setObjectName("reliefRepeatU");ru->setRange(1,1000);ru->setValue(12);form->addRow(DomTranslate("Repeats around"),ru);
    auto* rv=new QSpinBox(this);rv->setObjectName("reliefRepeatV");rv->setRange(1,1000);rv->setValue(1);form->addRow(DomTranslate("Repeats vertically"),rv);
    auto* rotation=spin("Pattern rotation (degrees)","reliefRotation",-360,360,0);
    auto* low=spin("Relief starts at height (%)","reliefStart",0,99.9,0);
    auto* high=spin("Relief ends at height (%)","reliefEnd",.1,100,100);
    auto* fade=spin("Edge fade (mm)","reliefFade",.05,1000,3);
    if(surface_mapping) {
        axis->setEnabled(false);form->labelForField(axis)->setEnabled(false);
        static_cast<QLabel*>(form->labelForField(ru))->setText(DomTranslate("Repeats across surface"));
        static_cast<QLabel*>(form->labelForField(rv))->setText(DomTranslate("Repeats along surface"));
        static_cast<QLabel*>(form->labelForField(low))->setText(DomTranslate("Relief starts along surface (%)"));
        static_cast<QLabel*>(form->labelForField(high))->setText(DomTranslate("Relief ends along surface (%)"));
        ru->setValue(1);
        help->setText(help->text()+"\n"+DomTranslate("The image follows the selected surface coordinates; the vase axis is not used."));
    }
    auto* invert=new QCheckBox(DomTranslate("Invert image heights"),this);invert->setObjectName("reliefInvert");layout->addWidget(invert);
    hide_source_=new QCheckBox(DomTranslate("Hide source Solid after creation"),this);hide_source_->setObjectName("reliefHideSource");hide_source_->setChecked(true);layout->addWidget(hide_source_);
    auto* note=new QLabel(DomTranslate("Open shells remain open. The preview is approximate. Final meshing refines only the selected face to the requested spacing."),this);
    note->setWordWrap(true);layout->addWidget(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel,this);layout->addWidget(buttons);
    auto* quick=new QPushButton(DomTranslate("1. Quick preview"),this);quick->setObjectName("previewReliefMesh");layout->insertWidget(layout->count()-1,quick);quick->setEnabled(false);
    auto* create=new QPushButton(DomTranslate("2. Build final mesh"),this);create->setObjectName("createReliefMesh");layout->insertWidget(layout->count()-1,create);create->setEnabled(false);
    auto invalidate=[=](){create->setEnabled(false);preview->SetMesh({},axis->currentIndex());summary->setText(DomTranslate("Preview needs rebuilding after changes."));};
    for(auto* s:{spacing,height,rotation,low,high,fade})connect(s,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double){invalidate();});
    for(auto* s:{ru,rv})connect(s,qOverload<int>(&QSpinBox::valueChanged),this,[=](int){invalidate();});
    connect(axis,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int){invalidate();});connect(invert,&QCheckBox::toggled,this,[=](bool){invalidate();});
    image_changed_=[=](const QImage& loaded){*image=loaded;invalidate();thumbnail->setPixmap(QPixmap::fromImage(loaded).scaled(400,75,Qt::KeepAspectRatio,Qt::SmoothTransformation));quick->setEnabled(!loaded.isNull()&&face_index>=0);};
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(image_button,&QPushButton::clicked,this,[=]() {
        QString path=QFileDialog::getOpenFileName(this,DomTranslate("Open ornament image..."),QSettings().value("tools/imageRelief/imagePath").toString(),"Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp)");
        if(path.isEmpty())return;
        QImageReader reader(path);reader.setAutoTransform(true);
        QSize size=reader.size();
        if(size.isValid()&&static_cast<qint64>(size.width())*size.height()>64000000) {
            QMessageBox::warning(this,windowTitle(),DomTranslate("Image is too large. Use at most 64 million pixels."));return;
        }
        QImage loaded=reader.read();
        if(loaded.isNull()) {QMessageBox::warning(this,windowTitle(),reader.errorString());return;}
        SetImage(loaded);image_button->setToolTip(path);
        QSettings().setValue("tools/imageRelief/imagePath",path);
    });
    auto build=[=,&solid](bool coarse) {
        if(busy_||face_index<0)return;
        if(low->value()>=high->value()) {QMessageBox::warning(this,windowTitle(),DomTranslate("The relief start must be below its end."));return;}
        image_relief::Options o;o.spacing=spacing->value();o.height=height->value();o.axis=axis->currentIndex();
        o.repeat_u=ru->value();o.repeat_v=rv->value();o.rotation_degrees=rotation->value();
        o.lower_percent=low->value();o.upper_percent=high->value();o.fade_mm=fade->value();o.invert=invert->isChecked();
        o.face_index=face_index;o.preview=coarse;o.allow_open=true;
        Vec3 lo{},hi{};solid.GetBounds(lo,hi);double diagonal=std::sqrt(std::pow(hi.x-lo.x,2)+std::pow(hi.y-lo.y,2)+std::pow(hi.z-lo.z,2));
        if(coarse){o.spacing=std::max(o.spacing,diagonal/55);o.max_triangles=60000;}
        busy_=true;controls->setEnabled(false);std::atomic<bool> cancelled{false};std::mutex mutex;std::string stage;
        cancel_build_=[&](){cancelled=true;};
        QProgressDialog progress(DomTranslate("Creating relief mesh..."),DomTranslate("Cancel"),0,0,this);
        progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);progress.show();
        connect(&progress,&QProgressDialog::canceled,this,[&](){cancelled=true;});
        auto future=std::async(std::launch::async,[&](){return image_relief::Build(solid.m_Shape,*image,o,[&](const char* text){std::lock_guard<std::mutex> lock(mutex);stage=text;return !cancelled.load();});});
        QEventLoop loop;QTimer timer;timer.setInterval(100);
        connect(&timer,&QTimer::timeout,&loop,[&](){std::string text;{std::lock_guard<std::mutex> lock(mutex);text=stage;}progress.setLabelText(cancelled?DomTranslate("Cancelling..."):DomTranslate(QString::fromStdString(text)));if(future.wait_for(std::chrono::seconds(0))==std::future_status::ready)loop.quit();});
        timer.start();loop.exec();timer.stop();auto built=future.get();disconnect(&progress,nullptr,this,nullptr);progress.close();cancel_build_={};busy_=false;controls->setEnabled(true);
        if(cancelled)return;
        if(!built.mesh) {
            if(built.error!="Cancelled")QMessageBox::warning(this,windowTitle(),DomTranslate(QString::fromStdString(built.error)));
            return;
        }
        if(coarse){summary->setText(DomTranslate("Coarse preview: %1 triangles; face edge up to %2 mm. %3. Drag to rotate.").arg(built.triangles).arg(built.max_base_edge,0,'f',2).arg(built.closed?DomTranslate("Closed mesh"):DomTranslate("Open shell")));preview->SetMesh(std::move(built.mesh),o.axis);create->setEnabled(true);return;}
        result_=std::move(built);
        result_.mesh->SetName(solid.GetName()+" Relief");
        result_.mesh->SetMaterial(solid.GetMaterial());result_.mesh->SetMaterialId(solid.GetMaterialId());result_.mesh->SetColor(solid.GetColor());
        accept();
    };
    connect(quick,&QPushButton::clicked,this,[=](){build(true);});connect(create,&QPushButton::clicked,this,[=](){build(false);});

    // Persist values, not a dialog containing references to an old CAD body.
    QSettings settings;settings.beginGroup("tools/imageRelief");
    for(auto* s:{spacing,height,rotation,low,high,fade}) {
        bool ok=false;double value=settings.value(s->objectName(),s->value()).toDouble(&ok);
        if(ok&&std::isfinite(value))s->setValue(value);
    }
    for(auto* s:{ru,rv})s->setValue(settings.value(s->objectName(),s->value()).toInt());
    axis->setCurrentIndex(std::clamp(settings.value("reliefAxis",2).toInt(),0,2));
    for(auto* c:{invert,hide_source_})c->setChecked(settings.value(c->objectName(),c->isChecked()).toBool());
    restoreGeometry(settings.value("geometry").toByteArray());
    const QString image_path=settings.value("imagePath").toString();
    if(!image_path.isEmpty()) {
        QImageReader reader(image_path);reader.setAutoTransform(true);const QSize size=reader.size();
        QImage restored;
        if(!size.isValid()||static_cast<qint64>(size.width())*size.height()<=64000000)restored=reader.read();
        if(!restored.isNull()){SetImage(restored);image_button->setToolTip(image_path);}
        else summary->setText(DomTranslate("Last ornament image is unavailable. Open another image."));
    }
    auto save=[=]() {
        QSettings saved;saved.beginGroup("tools/imageRelief");
        for(auto* s:{spacing,height,rotation,low,high,fade})saved.setValue(s->objectName(),s->value());
        for(auto* s:{ru,rv})saved.setValue(s->objectName(),s->value());
        saved.setValue("reliefAxis",axis->currentIndex());
        for(auto* c:{invert,hide_source_})saved.setValue(c->objectName(),c->isChecked());
        saved.setValue("geometry",saveGeometry());
    };
    for(auto* s:{spacing,height,rotation,low,high,fade})connect(s,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[=](double){save();});
    for(auto* s:{ru,rv})connect(s,qOverload<int>(&QSpinBox::valueChanged),this,[=](int){save();});
    connect(axis,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int){save();});
    for(auto* c:{invert,hide_source_})connect(c,&QCheckBox::toggled,this,[=](bool){save();});
    connect(this,&QDialog::finished,this,[=](int){save();});
}
void ImageReliefDialog::SetImage(const QImage& image){if(!busy_&&image_changed_)image_changed_(image);}
void ImageReliefDialog::reject(){if(busy_){if(cancel_build_)cancel_build_();return;}QDialog::reject();}
bool ImageReliefDialog::HideSource() const {return hide_source_->isChecked();}
