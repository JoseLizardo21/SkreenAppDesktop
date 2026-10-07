# Passed by build-rpm.sh from skreen_drive/VERSION
%{!?driver_version: %global driver_version 0.0.0}
%global dkms_name skreen-driver

Name:           skreenapp
Version:        0.1.0
Release:        1%{?dist}
Summary:        Desktop app for screen streaming to mobile via SkreenApp
License:        Proprietary

Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.10
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  pkgconfig(gstreamer-video-1.0)
BuildRequires:  pkgconfig(gstreamer-webrtc-1.0)
BuildRequires:  pkgconfig(gstreamer-sdp-1.0)
BuildRequires:  pkgconfig(nice)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  git

Requires:       gtk3
Requires:       gstreamer1
Requires:       gstreamer1-plugins-base
Requires:       gstreamer1-plugins-ugly
Requires:       gstreamer1-plugins-bad-free
Requires:       libnice-gstreamer1
Requires:       gstreamer1-vaapi
Requires:       android-tools
# Virtual monitor kernel driver, built by DKMS for every installed kernel
Requires:       dkms
Requires:       kernel-devel
Requires:       make
Requires:       gcc

%description
SkreenApp Desktop allows you to stream your desktop screen to a mobile
device using the SkreenApp mobile application. It includes the DRM kernel
driver that exposes the virtual monitor, built with DKMS so it is rebuilt
on every kernel update.

%prep
%autosetup -n %{name}-%{version}

%build
mkdir -p _build
cd _build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DSKREEN_DRIVER_DIR="$(pwd)/../driver"
%make_build

%install
install -Dm755 _build/skreen_desktop \
    %{buildroot}%{_bindir}/skreen_desktop

install -Dm644 src/assets/logo-removebg-preview.png \
    %{buildroot}%{_datadir}/pixmaps/skreenapp.png

install -Dm644 packaging/skreenapp.desktop \
    %{buildroot}%{_datadir}/applications/skreenapp.desktop

# Driver sources for DKMS (kernel sources only, no userspace tools)
dkms_dir=%{buildroot}%{_usrsrc}/%{dkms_name}-%{driver_version}
mkdir -p $dkms_dir
for f in driver/*.c driver/*.h; do
    [ "$(basename $f)" = monitor_control_ioctl.c ] && continue
    install -m644 $f $dkms_dir/
done
install -m644 driver/Makefile driver/VERSION $dkms_dir/
sed 's/#MODULE_VERSION#/%{driver_version}/' driver/dkms.conf > $dkms_dir/dkms.conf

# Load the module on every boot
install -Dm644 packaging/skreen-driver.conf \
    %{buildroot}%{_prefix}/lib/modules-load.d/skreen-driver.conf

%post
update-desktop-database %{_datadir}/applications &>/dev/null || :
# On upgrade the new %%post runs before the old %%preun: the new driver
# version is added and installed first, then the old one is removed from the
# DKMS tree.
dkms add -m %{dkms_name} -v %{driver_version} -q --rpm_safe_upgrade || :
dkms build -m %{dkms_name} -v %{driver_version} -q || :
dkms install -m %{dkms_name} -v %{driver_version} -q --force || :

%preun
if [ $1 -eq 0 ]; then
    modprobe -r skreen_driver &>/dev/null || :
fi
dkms remove -m %{dkms_name} -v %{driver_version} -q --all --rpm_safe_upgrade || :

%postun
update-desktop-database %{_datadir}/applications &>/dev/null || :

%posttrans
# Try to switch to the new module right away. While the compositor holds the
# DRM device, unloading fails and the new module is loaded on the next boot;
# the app detects the version mismatch and asks the user to reboot.
modprobe -r skreen_driver &>/dev/null || :
modprobe skreen_driver &>/dev/null || :

%files
%{_bindir}/skreen_desktop
%{_datadir}/pixmaps/skreenapp.png
%{_datadir}/applications/skreenapp.desktop
%{_usrsrc}/%{dkms_name}-%{driver_version}
%{_prefix}/lib/modules-load.d/skreen-driver.conf

%changelog
* Tue May 27 2026 Lizardo <jose.212002lizardo@gmail.com> - 0.1.0-1
- Initial RPM package
