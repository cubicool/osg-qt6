cmake_minimum_required(VERSION 3.16)
project(osg-qt6)

# Change this for MSVC, or you can specify it on the commandline via:
# `cmake -DCMAKE_PREFIX_PATH="C:/Qt/..."`
# set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64;D:/dev/local")

find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets Widgets)
find_package(OpenSceneGraph REQUIRED
	osg
	osgDB
	osgGA
	osgViewer
	osgUtil
	osgFX
	osgText
	osgWidget
	osgAnimation
)

set(CMAKE_AUTOMOC ON)

function(EXAMPLE_EXE name)
	add_executable(example-${name} "example-${name}.cpp")

	target_link_libraries(example-${name} PRIVATE
		Qt6::OpenGLWidgets
		Qt6::Widgets
		${OPENSCENEGRAPH_LIBRARIES}
		osgEarthd
		# GL
		# GLEW
		# gdal
	)
	target_include_directories(example-${name} PRIVATE
		${OPENSCENEGRAPH_INCLUDE_DIRS}
	)
	target_link_directories(example-${name} PRIVATE
		# "D:/Dev/local/lib"
		"/home/cubicool/local/lib"
	)
	target_compile_features(example-${name} PUBLIC cxx_std_20)
endfunction()

example_exe("gl")
example_exe("osg")
example_exe("osgearth")
example_exe("osgearth-interactive")
