Summary: IceCube DOMCal Surface Client 
Name: domcal-surface
Version: %{VER}
Release: %{REL}
URL: http://icecube.wisc.edu
Source0: %{name}-%{version}.tgz
License: Copyright 2013 IceCube Collaboration
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-root
Prefix: %{_prefix}

%description
IceCube DOMCal Surface Client

%prep

%setup -q

%build
cd surface; make clean; make DOMCal

%install
install -d ${RPM_BUILD_ROOT}/mnt/data/testdaq
install -d ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin

install surface/DOMCal ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin/
install resources/surface/domcal-wrapper ${RPM_BUILD_ROOT}/mnt/data/testdaq/bin/domcal-wrapper-cpp
install resources/surface/domcal_ic86.config ${RPM_BUILD_ROOT}/mnt/data/testdaq/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,testdaq,testdaq)
/mnt/data/testdaq/bin/DOMCal
/mnt/data/testdaq/bin/domcal-wrapper-cpp
/mnt/data/testdaq/domcal_ic86.config

%changelog
* Mon Feb 3 2014 John Kelley <jkelley@icecube.wisc.edu>
- Initial RPM build.


