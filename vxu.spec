Summary: command line utilities to read/write Yaesu/Vertex VX-5R transceiver
Name:	vxu
Version:	0.5.0
Release: 1
License:	MIT
Group:	Applications/System
URL:	http://www.OnJapan.net/ham/
Source0:	%{name}-%{version}.tgz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root


%description

vxur receives data sent from a Vertex Standard (Yaesu) VX-5R or
FT-817/FT-857 transceiver in "clone" mode into a yard/eve
compatible file.  vxuw writes a file into a transceiver in
"clone" mode.

%prep

%setup -q

%build
make CFLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1

make install \
	INSTALLDIR=$RPM_BUILD_ROOT%{_bindir} \
	MANDIR=$RPM_BUILD_ROOT%{_mandir}/man1 \


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%{_bindir}/vxur
%{_bindir}/vxuw
%{_mandir}/man1/vxur.1*
%{_mandir}/man1/vxuw.1*
%doc NEWS

%changelog
* Wed Feb 12 2003 Jim Tittsler <7J1AJH@OnJapan.net>
- update to 0.4.0
- include NEWS file
* Mon Dec 09 2002 Jim Tittsler <7J1AJH@OnJapan.net>
- update to 0.3
- change name from yardl/yawrl

