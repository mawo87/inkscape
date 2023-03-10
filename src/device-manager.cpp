// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Inkscape::DeviceManager - a view of input devices available.
 *
 * Copyright 2010  Jon A. Cruz  <jon@joncruz.org>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "device-manager.h"

#include <set>
#include <vector>

#include "preferences.h"

// Only needed for gtk_accelerator_parse(). Can probably drop when we switch to Gtk+ 4
#include <gtk/gtk.h>

#include <glibmm/regex.h>

#include <gdkmm/display.h>
#include <gdkmm/seat.h>

#include <gtkmm/accelkey.h>

#define noDEBUG_VERBOSE 1


// This is a copy of the private fields of the GdkDevice object, used in order
// to create a list of "fake" devices.
struct GdkDeviceFake {
        Glib::ustring    name;
        Gdk::InputSource source;
        Gdk::InputMode   mode;
	bool             has_cursor;
	int              num_axes;
	int              num_keys;
};


static void createFakeList();
static std::vector<GdkDeviceFake> fakeList;

static bool isValidDevice(Glib::RefPtr<Gdk::Device> device)
{
    bool valid = true;
    for (std::vector<GdkDeviceFake>::iterator it = fakeList.begin(); it != fakeList.end() && valid; ++it) {
	const bool name_matches     = (device->get_name()   == (*it).name);
	const bool source_matches   = (device->get_source() == (*it).source);
	const bool mode_matches     = (device->get_mode()   == (*it).mode);
	const bool num_axes_matches = (device->get_n_axes() == (*it).num_axes);
	const bool num_keys_matches = (device->get_n_keys() == (*it).num_keys);

	if (name_matches && source_matches && mode_matches 
			&& num_axes_matches && num_keys_matches)
		valid = false;
    }

    return valid;
}

namespace Inkscape {

static int const NUM_AXES = 24;
static const int RUNAWAY_MAX = 1000;

static Glib::ustring getBaseDeviceName(Gdk::InputSource source)
{
    Glib::ustring name;
    switch (source) {
        case Gdk::SOURCE_MOUSE:
            name = "pointer";
            break;
        case Gdk::SOURCE_PEN:
            name = "pen";
            break;
        case Gdk::SOURCE_ERASER:
            name = "eraser";
            break;
        case Gdk::SOURCE_CURSOR:
            name = "cursor";
            break;
        default:
            name = "tablet";
    }
    return name;
}

static std::map<Glib::ustring, Gdk::AxisUse> &getStringToAxis()
{
    static bool init = false;
    static std::map<Glib::ustring, Gdk::AxisUse> mapping;
    if (!init) {
        init = true;
        mapping["ignore"]   = Gdk::AXIS_IGNORE;
        mapping["x"]        = Gdk::AXIS_X;
        mapping["y"]        = Gdk::AXIS_Y;
        mapping["pressure"] = Gdk::AXIS_PRESSURE;
        mapping["xtilt"]    = Gdk::AXIS_XTILT;
        mapping["ytilt"]    = Gdk::AXIS_YTILT;
        mapping["wheel"]    = Gdk::AXIS_WHEEL;
    }
    return mapping;
}

static std::map<Gdk::AxisUse, Glib::ustring> &getAxisToString()
{
    static bool init = false;
    static std::map<Gdk::AxisUse, Glib::ustring> mapping;
    if (!init) {
        init = true;
        for (auto & it : getStringToAxis()) {
            mapping.insert(std::make_pair(it.second, it.first));
        }
    }
    return mapping;
}

static std::map<Glib::ustring, Gdk::InputMode> &getStringToMode()
{
    static bool init = false;
    static std::map<Glib::ustring, Gdk::InputMode> mapping;
    if (!init) {
        init = true;
        mapping["disabled"] = Gdk::MODE_DISABLED;
        mapping["screen"]   = Gdk::MODE_SCREEN;
        mapping["window"]   = Gdk::MODE_WINDOW;
    }
    return mapping;
}

static std::map<Gdk::InputMode, Glib::ustring> &getModeToString()
{
    static bool init = false;
    static std::map<Gdk::InputMode, Glib::ustring> mapping;
    if (!init) {
        init = true;
        for (auto & it : getStringToMode()) {
            mapping.insert(std::make_pair(it.second, it.first));
        }
    }
    return mapping;
}



InputDevice::InputDevice()
    : Glib::Object()
{}

InputDevice::~InputDevice() = default;

class InputDeviceImpl : public InputDevice {
public:
    InputDeviceImpl(Glib::RefPtr<Gdk::Device> device, std::set<Glib::ustring> &knownIDs);
    ~InputDeviceImpl() override = default;

    Glib::ustring getId() const override {return id;}
    Glib::ustring getName() const override {return name;}
    Gdk::InputSource getSource() const override {return source;}
    Gdk::InputMode getMode() const override {return (device->get_mode());}
    gint getNumAxes() const override {return device->get_n_axes();}
    bool hasCursor() const override {return device->get_has_cursor();}
    int getNumKeys() const override {return device->get_n_keys();}
    Glib::ustring getLink() const override {return link;}
    virtual void setLink( Glib::ustring const& link ) {this->link = link;}
    gint getLiveAxes() const override {return liveAxes;}
    virtual void setLiveAxes(gint axes) {liveAxes = axes;}
    gint getLiveButtons() const override {return liveButtons;}
    virtual void setLiveButtons(gint buttons) {liveButtons = buttons;}

    // internal methods not on public superclass:
    virtual Glib::RefPtr<Gdk::Device> getDevice() {return device;}

private:
    InputDeviceImpl(InputDeviceImpl const &) = delete; // no copy
    void operator=(InputDeviceImpl const &) = delete; // no assign

    static Glib::ustring createId(Glib::ustring const &id, Gdk::InputSource source, std::set<Glib::ustring> &knownIDs);

    Glib::RefPtr<Gdk::Device> device;
    Glib::ustring id;
    Glib::ustring name;
    Gdk::InputSource source;
    Glib::ustring link;
    guint liveAxes;
    guint liveButtons;
};

class IdMatcher
{
public:
    IdMatcher(Glib::ustring const& target):target(target) {}
    bool operator ()(Glib::RefPtr<InputDeviceImpl>& dev) {return dev && (target == dev->getId());}

private:
    Glib::ustring const& target;
};

class LinkMatcher
{
public:
    LinkMatcher(Glib::ustring const& target):target(target) {}
    bool operator ()(Glib::RefPtr<InputDeviceImpl>& dev) {return dev && (target == dev->getLink());}

private:
    Glib::ustring const& target;
};

InputDeviceImpl::InputDeviceImpl(Glib::RefPtr<Gdk::Device> device, std::set<Glib::ustring> &knownIDs)
    : InputDevice(),
      device(device),
      id(),
      name(!device->get_name().empty() ? device->get_name() : ""),
      source(device->get_source()),
      link(),
      liveAxes(0),
      liveButtons(0)
{
    id = createId(name, source, knownIDs);
}


Glib::ustring InputDeviceImpl::createId(Glib::ustring const &id,
                                        Gdk::InputSource source,
                                        std::set<Glib::ustring> &knownIDs)
{
    // Start with only allowing printable ASCII. Check later for more refinements.
    bool badName = id.empty() || !id.is_ascii();
    for (Glib::ustring::const_iterator it = id.begin(); (it != id.end()) && !badName; ++it) {
        badName = *it < 0x20;
    }

    Glib::ustring base;
    switch ( source ) {
        case Gdk::SOURCE_MOUSE:
            base = "M:";
            break;
        case Gdk::SOURCE_CURSOR:
            base = "C:";
            break;
        case Gdk::SOURCE_PEN:
            base = "P:";
            break;
        case Gdk::SOURCE_ERASER:
            base = "E:";
            break;
        default:
            base = "?:";
    }

    if (badName) {
        base += getBaseDeviceName(source);
    } else {
        base += id;
    }

    // now ensure that all IDs become unique in a session.
    int num = 1;
    Glib::ustring result = base;
    while ((knownIDs.find(result) != knownIDs.end()) && (num < RUNAWAY_MAX)) {
        result = Glib::ustring::compose("%1%2", base, ++num);
    }

    knownIDs.insert(result);
    return result;
}





class DeviceManagerImpl : public DeviceManager {
public:
    DeviceManagerImpl();

    void loadConfig() override;
    void saveConfig() override;

    std::list<Glib::RefPtr<InputDevice const> > getDevices() override;

    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalDeviceChanged() override;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalAxesChanged() override;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalButtonsChanged() override;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalLinkChanged() override;

    void addAxis(Glib::ustring const & id, gint axis) override;
    void addButton(Glib::ustring const & id, gint button) override;
    void setLinkedTo(Glib::ustring const & id, Glib::ustring const& link) override;

    void setMode( Glib::ustring const & id, Gdk::InputMode mode ) override;
    void setAxisUse( Glib::ustring const & id, guint index, Gdk::AxisUse use ) override;
    void setKey( Glib::ustring const & id, guint index, guint keyval, Gdk::ModifierType mods ) override;

protected:
    std::list<Glib::RefPtr<InputDeviceImpl> > devices;

    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalDeviceChangedPriv;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalAxesChangedPriv;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalButtonsChangedPriv;
    sigc::signal<void (Glib::RefPtr<InputDevice const> )> signalLinkChangedPriv;
};


DeviceManagerImpl::DeviceManagerImpl() :
    DeviceManager(),
    devices()
{
    auto display = Gdk::Display::get_default();
    auto seat    = display->get_default_seat();
    auto devList = seat->get_slaves(Gdk::SEAT_CAPABILITY_ALL);

    if (fakeList.empty()) {
        createFakeList();
    }
    //devList = fakeList;

    std::set<Glib::ustring> knownIDs;

    for (auto dev : devList) {
           // GTK+ 3 has added keyboards to the list of supported devices.
           if(dev->get_source() != Gdk::SOURCE_KEYBOARD) {

#if DEBUG_VERBOSE
               g_message("device: name[%s] source[0x%x] mode[0x%x] cursor[%s] axis count[%d] key count[%d]", dev->name, dev->source, dev->mode,
                       dev->has_cursor?"Yes":"no", dev->num_axes, dev->num_keys);
#endif

               InputDeviceImpl* device = new InputDeviceImpl(dev, knownIDs);
               device->reference();
               devices.emplace_back(device);
           }
    }
}

void DeviceManagerImpl::loadConfig()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    for (auto & device : devices) {
        if (device->getSource() != Gdk::SOURCE_MOUSE) {
            Glib::ustring path = "/devices/" + device->getId();

            Gdk::InputMode mode = Gdk::MODE_DISABLED;
            Glib::ustring val = prefs->getString(path + "/mode");
            if (getStringToMode().find(val) != getStringToMode().end()) {
                mode = getStringToMode()[val];
            }
            if (device->getMode() != mode) {
                setMode( device->getId(), mode );
            }

            //

            val = prefs->getString(path + "/axes");
            if (!val.empty()) {
                std::vector<Glib::ustring> parts = Glib::Regex::split_simple(";", val);
                for (size_t i = 0; i < parts.size(); ++i) {
                    Glib::ustring name = parts[i];
                    if (getStringToAxis().find(name) != getStringToAxis().end()) {
                        Gdk::AxisUse use = getStringToAxis()[name];
                        setAxisUse( device->getId(), i, use );
                    }
                }
            }

            val = prefs->getString(path + "/keys");
            if (!val.empty()) {
                std::vector<Glib::ustring> parts = Glib::Regex::split_simple(";", val);
                for (size_t i = 0; i < parts.size(); ++i) {
                    Glib::ustring keyStr = parts[i];
                    if (!keyStr.empty()) {
                        guint key = 0;
                        GdkModifierType mods = static_cast<GdkModifierType>(0);
                        gtk_accelerator_parse( keyStr.c_str(), &key, &mods );
                        setKey( device->getId(), i, key, static_cast<Gdk::ModifierType>(mods) );
                    }
                }
            }
        }
    }
}

void DeviceManagerImpl::saveConfig()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    for (auto & it : devices) {
        if (it->getSource() != Gdk::SOURCE_MOUSE) {
            Glib::ustring path = "/devices/" + it->getId();

            prefs->setString( path + "/mode", getModeToString()[it->getMode()].c_str() );

            Glib::ustring tmp;
            for (gint i = 0; i < it->getNumAxes(); ++i) {
                if (i > 0) {
                    tmp += ";";
                }
                Glib::RefPtr<Gdk::Device> device = it->getDevice();
                tmp += getAxisToString()[device->get_axis_use(i)];
            }
            prefs->setString( path + "/axes", tmp );

            tmp = "";
            for (gint i = 0; i < it->getNumKeys(); ++i) {
                if (i > 0) {
                    tmp += ";";
                }
                Glib::RefPtr<Gdk::Device> device = it->getDevice();
		guint keyval;
                Gdk::ModifierType modifiers;
		device->get_key(i, keyval, modifiers);
                Gtk::AccelKey accelkey(keyval, modifiers);
                tmp += accelkey.get_abbrev();
            }
            prefs->setString( path + "/keys", tmp );
        }
    }
}

std::list<Glib::RefPtr<InputDevice const> > DeviceManagerImpl::getDevices()
{
    std::list<Glib::RefPtr<InputDevice const> > tmp;
    for ( std::list<Glib::RefPtr<InputDeviceImpl> >::const_iterator it = devices.begin(); it != devices.end(); ++it ) {
        tmp.emplace_back(*it);
    }
    return tmp;
}

void DeviceManagerImpl::setMode( Glib::ustring const & id, Gdk::InputMode mode )
{
    std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
    if ( it != devices.end() ) {
        Glib::RefPtr<Gdk::Device> device = (*it)->getDevice();
        if (isValidDevice(device) && ((*it)->getMode() != mode) ) {
            bool success = device->set_mode(mode);
            if (success) {
                signalDeviceChangedPriv.emit(*it);
            } else {
                g_warning("Unable to set mode on extended input device [%s]", (*it)->getId().c_str());
            }
        }
    }
}

void DeviceManagerImpl::setAxisUse( Glib::ustring const & id, guint index, Gdk::AxisUse use )
{
    std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
    if ( it != devices.end() ) {
        if (isValidDevice((*it)->getDevice())) {
            if (static_cast<gint>(index) <= (*it)->getNumAxes()) {
                Glib::RefPtr<Gdk::Device> device = (*it)->getDevice();

                if (device->get_axis_use(index) != use) {
                    device->set_axis_use(index, use);
                    signalDeviceChangedPriv.emit(*it);
                }
            } else {
                g_warning("Invalid device axis number %d on extended input device [%s]", index, (*it)->getId().c_str());
            }
        }
    }
}

void DeviceManagerImpl::setKey( Glib::ustring const & id, guint index, guint keyval, Gdk::ModifierType mods )
{
    //static void setDeviceKey( GdkDevice* device, guint index, guint keyval, GdkModifierType modifiers )
    //

    std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
    if ( it != devices.end() ) {
        if (isValidDevice((*it)->getDevice())) {
            Glib::RefPtr<Gdk::Device> device = (*it)->getDevice();
            device->set_key(index, keyval, mods);
            signalDeviceChangedPriv.emit(*it);
        }
    }
}

sigc::signal<void (Glib::RefPtr<InputDevice const> )> DeviceManagerImpl::signalDeviceChanged()
{
    return signalDeviceChangedPriv;
}

sigc::signal<void (Glib::RefPtr<InputDevice const> )> DeviceManagerImpl::signalAxesChanged()
{
    return signalAxesChangedPriv;
}

sigc::signal<void (Glib::RefPtr<InputDevice const> )> DeviceManagerImpl::signalButtonsChanged()
{
    return signalButtonsChangedPriv;
}

sigc::signal<void (Glib::RefPtr<InputDevice const> )> DeviceManagerImpl::signalLinkChanged()
{
    return signalLinkChangedPriv;
}

void DeviceManagerImpl::addAxis(Glib::ustring const & id, gint axis)
{
    if ( axis >= 0 && axis < NUM_AXES ) {
        std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
        if ( it != devices.end() ) {
            gint mask = 1u << axis;
            if ( (mask & (*it)->getLiveAxes()) == 0 ) {
                (*it)->setLiveAxes((*it)->getLiveAxes() | mask);

                // Only signal if a new axis was added
                (*it)->reference();
                signalAxesChangedPriv.emit(*it);
            }
        }
    }
}

void DeviceManagerImpl::addButton(Glib::ustring const & id, gint button)
{
    if ( button >= 0 && button < NUM_AXES ) {
        std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
        if ( it != devices.end() ) {
            gint mask = 1u << button;
            if ( (mask & (*it)->getLiveButtons()) == 0 ) {
                (*it)->setLiveButtons((*it)->getLiveButtons() | mask);

                // Only signal if a new button was added
                (*it)->reference();
                signalButtonsChangedPriv.emit(*it);
            }
        }
    }
}

void DeviceManagerImpl::setLinkedTo(Glib::ustring const & id, Glib::ustring const& link)
{
    std::list<Glib::RefPtr<InputDeviceImpl> >::iterator it = std::find_if(devices.begin(), devices.end(), IdMatcher(id));
    if ( it != devices.end() ) {
        Glib::RefPtr<InputDeviceImpl> dev = *it;

        Glib::RefPtr<InputDeviceImpl> targetDev;
        if ( !link.empty() ) {
            // Need to be sure the target of the link exists
            it = std::find_if(devices.begin(), devices.end(), IdMatcher(link));
            if ( it != devices.end() ) {
                targetDev = *it;
            }
        }


        if ( (link.empty() && !dev->getLink().empty())
             || (targetDev && (targetDev->getLink() != id)) ) {
            // only muck about if they aren't already linked
            std::list<Glib::RefPtr<InputDeviceImpl> > changedItems;

            if ( targetDev ) {
            // Is something else already using that link?
                it = std::find_if(devices.begin(), devices.end(), LinkMatcher(link));
                if ( it != devices.end() ) {
                    (*it)->setLink("");
                    changedItems.push_back(*it);
                }
            }
            it = std::find_if(devices.begin(), devices.end(), LinkMatcher(id));
            if ( it != devices.end() ) {
                (*it)->setLink("");
                changedItems.push_back(*it);
            }
            if ( targetDev ) {
                targetDev->setLink(id);
                changedItems.push_back(targetDev);
            }
            dev->setLink(link);
            changedItems.push_back(dev);

            for ( std::list<Glib::RefPtr<InputDeviceImpl> >::const_iterator iter = changedItems.begin(); iter != changedItems.end(); ++iter ) {
                (*iter)->reference();
                signalLinkChangedPriv.emit(*iter);
            }
        }
    }
}






static DeviceManagerImpl* theInstance = nullptr;

DeviceManager::DeviceManager()
    : Glib::Object()
{
}

DeviceManager::~DeviceManager() = default;

DeviceManager& DeviceManager::getManager() {
    if ( !theInstance ) {
        theInstance = new DeviceManagerImpl();
    }

    return *theInstance;
}

} // namespace Inkscape


static void createFakeList() {
    if (fakeList.empty()) {
        fakeList.resize(5);
        fakeList[0].name       = "pad";
        fakeList[0].source     = Gdk::SOURCE_PEN;
        fakeList[0].mode       = Gdk::MODE_SCREEN;
        fakeList[0].has_cursor = true;
        fakeList[0].num_axes   = 6;
        fakeList[0].num_keys   = 8;

        fakeList[1].name       = "eraser";
        fakeList[1].source     = Gdk::SOURCE_ERASER;
        fakeList[1].mode       = Gdk::MODE_SCREEN;
        fakeList[1].has_cursor = true;
        fakeList[1].num_axes   = 6;
        fakeList[1].num_keys   = 7;

        fakeList[2].name       = "cursor";
        fakeList[2].source     = Gdk::SOURCE_CURSOR;
        fakeList[2].mode       = Gdk::MODE_SCREEN;
        fakeList[2].has_cursor = true;
        fakeList[2].num_axes   = 6;
        fakeList[2].num_keys   = 7;

        fakeList[3].name       = "stylus";
        fakeList[3].source     = Gdk::SOURCE_PEN;
        fakeList[3].mode       = Gdk::MODE_SCREEN;
        fakeList[3].has_cursor = true;
        fakeList[3].num_axes   = 6;
        fakeList[3].num_keys   = 7;

        // try to find the first *real* core pointer
        auto display = Gdk::Display::get_default();
        auto seat    = display->get_default_seat();
        auto devList = seat->get_slaves(Gdk::SEAT_CAPABILITY_ALL);

        // Set iterator to point at beginning of device list
        std::vector< Glib::RefPtr<Gdk::Device> >::iterator dev = devList.begin();

        // Skip past any items in the device list that are not mice
        while (dev != devList.end() && (*dev)->get_source() != Gdk::SOURCE_MOUSE) {
            ++dev;
        }

        if (dev != devList.end()) {
            Glib::RefPtr<Gdk::Device> device = *dev;
            fakeList[4].name       = device->get_name();
            fakeList[4].source     = device->get_source();
            fakeList[4].mode       = device->get_mode();
            fakeList[4].has_cursor = device->get_has_cursor();
            fakeList[4].num_axes   = device->get_n_axes();
            fakeList[4].num_keys   = device->get_n_keys();
        } else {
            fakeList[4].name       = "Core Pointer";
            fakeList[4].source     = Gdk::SOURCE_MOUSE;
            fakeList[4].mode       = Gdk::MODE_SCREEN;
            fakeList[4].has_cursor = true;
            fakeList[4].num_axes   = 2;
            fakeList[4].num_keys   = 0;
        }
    }
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
