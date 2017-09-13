Name:		artik-ota
Summary:	OTA for ARTIK
Version:	1.0.3
Release:	1
Group:		System
License:	Apache-2.0

Source0:	%{name}-%{version}.tar.gz

Requires:	systemd
Requires:	dnf-automatic
Requires:	/etc/fstab
BuildRequires:	glib2

%description
OTA for ARTIK

%prep
%setup -q

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/etc/systemd/system
cp units/dnf-automatic.service %{buildroot}/etc/systemd/system

mkdir -p %{buildroot}/usr/lib/systemd/system
cp units/dnf-automatic.path %{buildroot}/usr/lib/systemd/system
cp units/dnf-automatic-undo.service %{buildroot}/usr/lib/systemd/system

mkdir -p %{buildroot}/usr/bin/
cp artik-updater %{buildroot}/usr/bin/

%post
/usr/bin/systemctl enable dnf-automatic.path
/usr/bin/systemctl disable dnf-automatic.timer
/usr/bin/systemctl enable dnf-automatic-undo.service

sed -i 's/apply_updates = no/apply_updates = yes/g' /etc/dnf/automatic.conf

echo "/dev/mmcblk0p7  /   ext4    errors=remount-ro,noatime,nodiratime    0   1" > /etc/fstab

%files
%attr(0644,root,root) /etc/systemd/system/dnf-automatic.service
%attr(0644,root,root) /usr/lib/systemd/system/dnf-automatic.path
%attr(0644,root,root) /usr/lib/systemd/system/dnf-automatic-undo.service
%attr(0755,root,root) /usr/bin/artik-updater

%build
make %{?_smp_mflags}
