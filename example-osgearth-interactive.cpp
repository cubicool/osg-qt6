#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QApplication>
#include <QMainWindow>
#include <QTimer>
#include <QMouseEvent>

#include <osgDB/ReadFile>

#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>

#include <osgEarth/MapNode>
#include <osgEarth/ImageLayer>
#include <osgEarth/GDAL>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgEarth/LatLongFormatter>

class ClickToLatLonHandler: public osgGA::GUIEventHandler {
public:
	ClickToLatLonHandler(osgEarth::MapNode* mapNode): _mapNode(mapNode) {}

	virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa) override {
		if(
			!_mapNode ||
			ea.getEventType() != osgGA::GUIEventAdapter::PUSH ||
			ea.getButton() != osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON
		) return false;

		auto* view = dynamic_cast<osgViewer::View*>(aa.asView());

		if(!view) return false;

		if(
			osg::Vec3d wp;
			_mapNode->getTerrain()->getWorldCoordsUnderMouse(view, ea.getX(), ea.getY(), wp)
		) {
			osgEarth::GeoPoint gp;

			gp.fromWorld(_mapNode->getMapSRS(), wp);

			OSG_WARN << "LatLong: " << osgEarth::Util::LatLongFormatter(
				osgEarth::Util::LatLongFormatter::AngularFormat::FORMAT_DECIMAL_DEGREES,
				osgEarth::Util::LatLongFormatter::Options::USE_SUFFIXES
			).format(gp) << std::endl;

			return true;
		}

		else OSG_WARN << "Failed to get LatLon for: " << ea.getX() << "x" << ea.getY() << std::endl;

		return false;
	}

private:
	osgEarth::MapNode* _mapNode;
};

class OSGWidget: public QOpenGLWidget, protected QOpenGLFunctions {
Q_OBJECT

public:
	OSGWidget(QWidget* parent=nullptr):
	QOpenGLWidget(parent) {
		_timer = new QTimer(this);

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
					button = osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON;

					break;

				case Qt::MiddleButton:
					button = osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON;

					break;

				case Qt::RightButton:
					button = osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON;

					break;

				default:
					break;
			}
		}

		std::string buttonName() const {
			if(button == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) return "LEFT_MOUSE";

			else if(button == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON) return "RIGHT_MOUSE";

			else return "";
		}

		friend std::ostream& operator<<(std::ostream& os, const MouseEventData& med) {
			return os << med.x << "x" << med.y << " (" << med.buttonName() << ")";
		}

		float x = 0.0f;
		float y = 0.0f;

		// TODO: This isn't really ideal, and is the ENTIRE REASON "enum class" exists (to prevent these
		// kinds of shenanigans); it'll have to do for now.
		osgGA::GUIEventAdapter::MouseButtonMask button = static_cast<osgGA::GUIEventAdapter::MouseButtonMask>(0);
	};

protected:
	void initializeGL() override {
		OSG_WARN << "initializeGL" << std::endl;

		osgEarth::initialize();

		osgEarth::Map* map = new osgEarth::Map();

		osgEarth::GDALImageLayer* imagery = new osgEarth::GDALImageLayer();

		imagery->setURL("../world.tif");

		map->addLayer(imagery);

		osgEarth::MapNode* node = new osgEarth::MapNode(map);

		_viewer = new osgViewer::Viewer();

		_viewer->setUpViewerAsEmbeddedInWindow(0, 0, width(), height());
		_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
		_viewer->setCameraManipulator(new osgEarth::EarthManipulator());
		_viewer->addEventHandler(new ClickToLatLonHandler(node));
		_viewer->setSceneData(node);

		osgEarth::MapNodeHelper().configureView(_viewer);
	}

	void resizeGL(int w, int h) override {
		OSG_WARN << "resizeGL: " << w << " x " << h << std::endl;

		_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, w, h));
		// _viewer->getCamera()->setProjectionMatrixAsPerspective(30.0f, static_cast<double>(w) / h, 1.0, 1000.0);
	}

	void paintGL() override {
		// OSG_WARN << "paintGL" << std::endl;

		_viewer->getCamera()->getGraphicsContext()->setDefaultFboId(defaultFramebufferObject());
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
		int delta = static_cast<float>(event->angleDelta().y()) / 120.0f;

		OSG_WARN << "wheelEvent: " << delta << std::endl;

		_viewer->getEventQueue()->mouseScroll(
			delta > 0 ?
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

	auto* osgWidget = new OSGWidget();

	mainWindow.setCentralWidget(osgWidget);
	mainWindow.resize(800, 600);
	mainWindow.show();

	return app.exec();
}

#include "example-osgearth-interactive.moc"
