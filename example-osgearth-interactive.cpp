//vimrun! ./example-osgearth-interactive

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
#include <osgEarth/PlaceNode>
#include <osgEarth/LocalGeometryNode>

#if 0
#include <ranges>

template<std::ranges::input_range R, typename T>
constexpr bool contains(R&& r, const T& value) {
	return std::ranges::find(r, value) != std::ranges::end(r);
}

class MouseDebugHandler: public osgGA::GUIEventHandler {
public:
	virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa) override {
		/* if(
			ea.getEventType() != osgGA::GUIEventAdapter::PUSH ||
			ea.getButton() != osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON
		) return false; */

		if(!contains(std::array{
			osgGA::GUIEventAdapter::PUSH,
			osgGA::GUIEventAdapter::RELEASE,
			osgGA::GUIEventAdapter::DRAG
		}, ea.getEventType())) return false;

		OE_WARN << " >> MouseDebug: "
			<< "event=" << ea.getEventType()
			<< ", button=" << ea.getButton()
			<< ", x=" << ea.getX()
			<< ", y=" << ea.getY()
			<< std::endl
		;

		return false;
	}
};
#endif

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

			OE_WARN << "LatLong: " << osgEarth::Util::LatLongFormatter(
				osgEarth::Util::LatLongFormatter::AngularFormat::FORMAT_DECIMAL_DEGREES,
				osgEarth::Util::LatLongFormatter::Options::USE_SUFFIXES
			).format(gp) << std::endl;

			addIcon(gp);

			return true;
		}

		else OE_WARN << "Failed to get LatLon for: " << ea.getX() << "x" << ea.getY() << std::endl;

		return false;
	}

	void addIcon(const osgEarth::GeoPoint& gp) {
		osgEarth::Style pm;

		auto* is = pm.getOrCreate<osgEarth::IconSymbol>();
		auto scale = 0.5;

		is->url().mutable_value().setLiteral("../blackdot.png");
		is->declutter() = false;
		is->scale() = scale;
		is->alignment() = osgEarth::IconSymbol::ALIGN_CENTER_CENTER;

		auto* ts = pm.getOrCreate<osgEarth::TextSymbol>();

		ts->size() = 96.0 * scale;
		ts->halo() = osgEarth::Color("#000000");
		ts->fill() = osgEarth::Color::White;
		ts->alignment() = osgEarth::TextSymbol::ALIGN_LEFT_CENTER;

		auto* pn = new osgEarth::PlaceNode(gp, "foo", pm);

		_mapNode->addChild(pn);
	}

private:
	osgEarth::MapNode* _mapNode;
};

#if 0
class TriangleNode: public osgEarth::LocalGeometryNode {
public:
	TriangleNode(const osgEarth::GeoPoint& gp, const osgEarth::Distance& d) {
		auto* geom = new osgEarth::Polygon();
		auto s = d.as(osgEarth::Units::METERS) / 2.0;

		geom->push_back(-s, -s, gp.z());
		geom->push_back(s, -s, gp.z());
		geom->push_back(0.0, s, gp.z());

		setGeometry(geom);
		setPosition(gp);
	}
};
#endif

class OSGWidget: public QOpenGLWidget, protected QOpenGLFunctions {
Q_OBJECT

public:
	OSGWidget(QWidget* parent=nullptr):
	QOpenGLWidget(parent) {
		QSurfaceFormat format;

		format.setRenderableType(QSurfaceFormat::OpenGL);
		format.setProfile(QSurfaceFormat::CompatibilityProfile);
		format.setSamples(4);

		setMouseTracking(true);
		setAttribute(Qt::WA_MouseTracking);
		// setMinimumSize(320, 240);
		setFocusPolicy(Qt::StrongFocus);
		setFormat(format);

		_timer = new QTimer(this);

		connect(_timer, &QTimer::timeout, this, QOverload<>::of(&OSGWidget::update));

		_timer->start(1000 / 60);
	}

	struct MouseEventData {
		// NOTE: We include the HEIGHT so we can account for differences between the QT windows
		// coords and the OSG window coords. HOWEVER... it doesn't seem to matter whether we do or
		// not; it still doesn't work (and actually BREAKS OE3's EarthManipulator Y axis)!
		MouseEventData(QMouseEvent* event, float height) {
			x = event->position().x();
			y = height - event->position().y();
			// y = event->position().y();

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
		OE_WARN << "initializeGL; dpr=" << devicePixelRatio() << std::endl;

		osgEarth::initialize();

		// NOTE: NOT SUPPOSED to use `auto*` with the Map! :) How does this work?
		auto* map = new osgEarth::Map();
		auto* imagery = new osgEarth::GDALImageLayer();

		imagery->setURL("../world.tif");

		map->addLayer(imagery);

		auto* node = new osgEarth::MapNode(map);

#if 0
		// ======================================
		osgEarth::Style style;

		style.getOrCreate<osgEarth::PolygonSymbol>()->fill().mutable_value().color() = osgEarth::Color::Red;
		style.getOrCreate<osgEarth::AltitudeSymbol>()->clamping() = osgEarth::AltitudeSymbol::CLAMP_TO_TERRAIN;
		style.getOrCreate<osgEarth::AltitudeSymbol>()->technique() = osgEarth::AltitudeSymbol::TECHNIQUE_DRAPE;

		auto* tri = new TriangleNode(
			osgEarth::GeoPoint(node->getMapSRS(), -117.172, 32.721),
			osgEarth::Distance(1000, osgEarth::Units::KILOMETERS)
			// osgEarth::Distance(100, osgEarth::Units::METERS)
		);

		tri->setStyle(style);
		tri->setScale(osg::Vec3d(0.5, 0.5, 1.0));

		node->addChild(tri);
		// =====================================
#endif

		_viewer = new osgViewer::Viewer();

		_gw = _viewer->setUpViewerAsEmbeddedInWindow(0, 0, width(), height());

		// NOTE: the setUpViewerAsEmbeddedInWindow set single-threaded for us.
		// _viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
		_viewer->setCameraManipulator(new osgEarth::EarthManipulator());
		_viewer->addEventHandler(new ClickToLatLonHandler(node));
		// _viewer->addEventHandler(new MouseDebugHandler());
		_viewer->setSceneData(node);

		osgEarth::MapNodeHelper().configureView(_viewer);
	}

	void resizeGL(int w, int h) override {
		OE_WARN << "resizeGL: " << w << " x " << h << std::endl;

		_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, w, h));
		_viewer->getCamera()->setProjectionMatrixAsPerspective(30.0f, static_cast<double>(w) / h, 1.0, 1000.0);
		_viewer->getEventQueue()->windowResize(0, 0, w, h);

		_gw->resized(0, 0, w, h);
	}

	void paintGL() override {
		// OE_WARN << "paintGL" << std::endl;

		_viewer->getCamera()->getGraphicsContext()->setDefaultFboId(defaultFramebufferObject());
		_viewer->frame();
	}

	auto _mouseEventData(QMouseEvent* event) const {
		return MouseEventData(event, height());
	}

	void keyPressEvent(QKeyEvent* event) override {
		if(event->key() == Qt::Key_Space) {
			OE_WARN << "keyPressEvent: " << event->key() << std::endl;

			double zoom = 500.0 + (4.0 * std::pow(2, (22 - std::clamp(static_cast<int>(15), 0, 22))));

			osgEarth::Viewpoint vp(
				"Target", // Name (optional, used for reference)
				-76.0,
				39.0,
				0.0, // Altitude
				0.0, // Heading (azimuth, degrees, 0 = north)
				-90.0, // Pitch (tilt, degrees, -90 = straight down)
				5000 // Range from target in meters (eye-to-ground distance)
			);

			auto* manip = dynamic_cast<osgEarth::Util::EarthManipulator*>(_viewer->getCameraManipulator());

			if(!manip) return;

			OE_WARN << "setViewpoint" << std::endl;

			manip->setViewpoint(vp, 1.0);

			// _viewer->getEventQueue()->keyPress(key, event->key());
		}
	}

	void mousePressEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OE_WARN << "mousePressEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseButtonPress(med.x, med.y, med.button);
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OE_WARN << "mouseMoveEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseMotion(med.x, med.y);
	}

	void mouseReleaseEvent(QMouseEvent* event) override {
		auto med = _mouseEventData(event);

		OE_WARN << "mouseReleaseEvent: " << med << std::endl;

		_viewer->getEventQueue()->mouseButtonRelease(med.x, med.y, med.button);
	}

	void wheelEvent(QWheelEvent* event) override {
		auto delta = static_cast<float>(event->angleDelta().y()) / 120.0f;

		OE_WARN << "wheelEvent: " << delta << std::endl;

		_viewer->getEventQueue()->mouseScroll(
			delta > 0.0f ?
			osgGA::GUIEventAdapter::SCROLL_UP :
			osgGA::GUIEventAdapter::SCROLL_DOWN
		);
	}

private:
	osg::ref_ptr<osgViewer::Viewer> _viewer;

	osgViewer::GraphicsWindowEmbedded* _gw;

	QTimer* _timer = nullptr;
};

int main(int argc, char** argv) {
	// QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

	QApplication app(argc, argv);
	QMainWindow mainWindow;

	auto* osgWidget = new OSGWidget();

	mainWindow.setCentralWidget(osgWidget);
	mainWindow.resize(800, 600);
	mainWindow.show();

	return app.exec();
}

#include "example-osgearth-interactive.moc"
