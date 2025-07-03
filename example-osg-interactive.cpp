#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QMainWindow>
#include <QTimer>
#include <QMouseEvent>

#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Point>
#include <osg/PolygonMode>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>

namespace osg_qt6 {

using vec_t = osg::Vec3::value_type;

osg::ShapeDrawable* createSphere(vec_t radius, vec_t pSize) {
	osg::ShapeDrawable* s = new osg::ShapeDrawable(new osg::Sphere(
		osg::Vec3(0.0, 0.0, 0.0),
		radius
	));

	s->getOrCreateStateSet()->setAttribute(
		new osg::Point(pSize),
		osg::StateAttribute::ON
	);

	return s;
}

osg::MatrixTransform* createSphereAt(const osg::Vec3& pos, vec_t radius, vec_t pSize) {
	osg::MatrixTransform* m = new osg::MatrixTransform(osg::Matrix::translate(pos));
	osg::Geode* g = new osg::Geode();

	g->addDrawable(createSphere(radius, pSize));

	m->addChild(g);

	return m;
}

}

class OSGWidget: public QOpenGLWidget, protected QOpenGLFunctions {

Q_OBJECT

public:
	OSGWidget(QWidget* parent=nullptr):
	QOpenGLWidget(parent) {
		_timer = new QTimer(this);

		// connect(_timer, &QTimer::timeout, this, &OSGWidget::update);
		// connect(_timer, &QTimer::timeout, this, QOverload<>::of(&OSGWidget::repaint));
		connect(_timer, &QTimer::timeout, this, QOverload<>::of(&OSGWidget::update));

		_timer->start(1000 / 60);
	}

	struct MouseEventData {
		// NOTE: We include the HEIGHT so we can account for differences between the QT windows
		// coords and the OSG window coords.
		MouseEventData(QMouseEvent* event, float height) {
			x = event->position().x();
			y = height - event->position().y();

			switch(event->button()) {
				case Qt::LeftButton:
					button = 1; // osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON;

					break;

				case Qt::MiddleButton:
					button = 2; // osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON;

					break;

				case Qt::RightButton:
					button = 3; // osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON;

					break;

				default:
					break;
			}
		}

		std::string buttonName() const {
			// if(button == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) return "LEFT_MOUSE";
			if(button == 1) return "LEFT_MOUSE";

			// else if(button == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON) return "RIGHT_MOUSE";
			else if(button == 3) return "RIGHT_MOUSE";

			else return "MIDDLE_MOUSE";
		}

		friend std::ostream& operator<<(std::ostream& os, const MouseEventData& med) {
			// return os << med.x << "x" << med.y << " (" << med.buttonName() << ")";
			return os << med.x << "x" << med.y << " (" << med.button << ")";
		}

		float x = 0.0f;
		float y = 0.0f;

		// TODO: This isn't really ideal, and is the ENTIRE REASON "enum class" exists (to prevent these
		// kinds of shenanigans); it'll have to do for now.
		// osgGA::GUIEventAdapter::MouseButtonMask button = static_cast<osgGA::GUIEventAdapter::MouseButtonMask>(0);
		unsigned int button = 0;
	};

protected:
	void initializeGL() override {
		OSG_WARN << "initializeGL; DPR = " << devicePixelRatio() << std::endl;

		osg::Group* root = new osg::Group();

		root->addChild(osg_qt6::createSphereAt(osg::Vec3(), 10.0, 2.0));
		root->getOrCreateStateSet()->setAttributeAndModes(
			new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::POINT),
			osg::StateAttribute::ON
		);

		_viewer = new osgViewer::Viewer();

		_viewer->setUpViewerAsEmbeddedInWindow(0, 0, width(), height());
		_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
		_viewer->setSceneData(root);
		_viewer->setCameraManipulator(new osgGA::TrackballManipulator());

		// TODO: Just letting QT6 "do its own thing" here seems to work much better. Need to
		// investigate this for more complex setups.
		/* osg::GraphicsContext::Traits* traits = new osg::GraphicsContext::Traits();

		traits->x = 0;
		traits->y = 0;
		traits->width = width();
		traits->height = height();
		traits->windowDecoration = false;
		traits->doubleBuffer = true;
		traits->sharedContext = 0;
		traits->setInheritedWindowPixelFormat = true;

		osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits);

		osg::Camera* camera = _viewer->getCamera();

		camera->setGraphicsContext(gc.get());
		camera->setViewport(new osg::Viewport(0, 0, width(), height()));
		camera->setProjectionMatrixAsPerspective(30.0f, static_cast<double>(width()) / height(), 1.0, 1000.0); */
	}

	void resizeGL(int w, int h) override {
		OSG_WARN << "resizeGL: " << w << " x " << h << std::endl;

		_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, w, h));
		_viewer->getCamera()->setProjectionMatrixAsPerspective(30.0f, static_cast<double>(w) / h, 1.0, 1000.0);
	}

	void paintGL() override {
		OSG_WARN << "paintGL" << std::endl;

		_viewer->frame();
	}

	auto _mouseEventData(QMouseEvent* event) const {
		return MouseEventData(event, height());
	}

	void mousePressEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OSG_WARN << "mousePressEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseButtonPress(med.x, med.y, med.button);
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OSG_WARN << "mouseMoveEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseMotion(med.x, med.y);
	}

	void mouseReleaseEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OSG_WARN << "mouseReleaseEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseButtonRelease(med.x, med.y, med.button);
	}

	void wheelEvent(QWheelEvent* event) override {
		auto delta = static_cast<float>(event->angleDelta().y()) / 120.0f;

		OSG_WARN << "wheelEvent: " << delta << std::endl;

		_viewer->getEventQueue()->mouseScroll(
			delta > 0.0f ?
			osgGA::GUIEventAdapter::SCROLL_UP :
			osgGA::GUIEventAdapter::SCROLL_DOWN
		);
	}

private:
	osg::ref_ptr<osgViewer::Viewer> _viewer;

	QTimer* _timer = nullptr;
};

int main(int argc, char** argv) {
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

	QApplication app(argc, argv);

	QMainWindow mainWindow;

	OSGWidget* osgWidget = new OSGWidget();

	mainWindow.setCentralWidget(osgWidget);
	mainWindow.resize(800, 600);
	mainWindow.show();

	return app.exec();
}

#include "example-osg-interactive.moc"
