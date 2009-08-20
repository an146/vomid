TEMPLATE = app
TARGET =
DEPENDPATH += libvomid/include
INCLUDEPATH += libvomid/include
MAKEFILE = Makefile.qmake

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build
QMAKE_LINK_OBJECT_SCRIPT = build/object_script

# Input
HEADERS += src/*.h
FORMS += src/*.ui
SOURCES += src/*.cpp
# RESOURCES += resources.qrc
# ICON += app.icns
# RC_FILE += resources.rc
LIBS += -Llibvomid/build -lvomid $$system("awk -F '+=' '/LDFLAGS\+\=/ {print $2;}' libvomid/build/config.mk")

# Install
target.path = $$(PREFIX)/bin
INSTALLS += target

# Mac OS X deployment
macx {
	CONFIG+=x86 ppc
	QMAKE_MACOSX_DEPLOYMENT_TARGET += 10.4
	QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk
}
