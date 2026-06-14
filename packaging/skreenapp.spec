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

%description
SkreenApp Desktop allows you to stream your desktop screen to a mobile
device using the SkreenApp mobile application.

%prep
%autosetup -n %{name}-%{version}

%build
mkdir -p _build
cd _build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix}
%make_build

%install
install -Dm755 _build/skreen_desktop \
    %{buildroot}%{_bindir}/skreen_desktop

install -Dm644 src/assets/logo-removebg-preview.png \
    %{buildroot}%{_datadir}/pixmaps/skreenapp.png

install -Dm644 packaging/skreenapp.desktop \
    %{buildroot}%{_datadir}/applications/skreenapp.desktop

%post
update-desktop-database %{_datadir}/applications &>/dev/null || :

%postun
update-desktop-database %{_datadir}/applications &>/dev/null || :

%files
%{_bindir}/skreen_desktop
%{_datadir}/pixmaps/skreenapp.png
%{_datadir}/applications/skreenapp.desktop

%changelog
* Tue May 27 2026 Lizardo <jose.212002lizardo@gmail.com> - 0.1.0-1
- Initial RPM package
