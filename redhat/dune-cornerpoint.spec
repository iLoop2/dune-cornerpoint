#
# spec file for package dune-cornerpoint
#

%define tag rc3

Name:           dune-cornerpoint
Version:        2013.10
Release:        0
Summary:        Cornerpoint grid management module for DUNE
License:        GPL-3.0
Group:          Development/Libraries/C and C++
Url:            http://www.opm-project.org/
Source0:        https://github.com/OPM/%{name}/archive/release/%{version}/%{tag}.tar.gz#/%{name}-%{version}.tar.gz
BuildRequires:  blas-devel lapack-devel dune-common-devel boost148-devel
BuildRequires:  git suitesparse-devel cmake28 doxygen bc ert.ecl-devel
BuildRequires:  tinyxml-devel dune-istl-devel opm-core-devel dune-grid-devel
%{?el5:BuildRequires: gcc44 gcc44-c++}
%{!?el5:BuildRequires: gcc gcc-c++}
BuildRequires:  opm-parser-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Requires:       libdune-cornerpoint1 = %{version}

%description
This module enables working with corner-point or, more
generally, pillar grids.  A standard grid type in the petroleum
industry, corner-point grids fill space with a relatively low number
of cells while still providing sufficient flexibility to model faults,
fractures and erosion.  The grid format was originally designed with
an eye towards geological modelling rather than numerical simulation
and this design choice does limit the number of feasible numerical
methods.

%package -n libdune-cornerpoint1
Summary:        Cornerpoint grid management module for DUNE
Group:          System/Libraries
%{?el5:BuildArch: %{_arch}}

%description -n libdune-cornerpoint1
This module enables working with corner-point or, more
generally, pillar grids.  A standard grid type in the petroleum
industry, corner-point grids fill space with a relatively low number
of cells while still providing sufficient flexibility to model faults,
fractures and erosion.  The grid format was originally designed with
an eye towards geological modelling rather than numerical simulation
and this design choice does limit the number of feasible numerical
methods.

%package devel
Summary:        Development and header files for dune-cornerpoint
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}
Requires:       blas-devel
Requires:       lapack-devel
Requires:       suitesparse-devel
Requires:       libdune-cornerpoint1 = %{version}
%{?el5:BuildArch: %{_arch}}

%description devel
This package contains the development and header files for dune-cornerpoint

%package doc
Summary:        Documentation files for dune-cornerpoint
Group:          Documentation
BuildArch:	noarch

%description doc
This package contains the documentation files for dune-cornerpoint

%package bin
Summary:        Applications in dune-cornerpoint
Group:          Scientific
Requires:       %{name} = %{version}
Requires:       libdune-cornerpoint1 = %{version}
%{?el5:BuildArch: %{_arch}}

%description bin
This package contains the applications for dune-cornerpoint

%prep
%setup -q -n %{name}-release-%{version}-%{tag}

# consider using -DUSE_VERSIONED_DIR=ON if backporting
%build
cmake28 -DBUILD_SHARED_LIBS=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_INSTALL_DOCDIR=share/doc/%{name}-%{version} -DUSE_RUNPATH=OFF %{?el5:-DCMAKE_CXX_COMPILER=g++44 -DCMAKE_C_COMPILER=gcc44 -DCMAKE_Fortran_COMPILER=gfortran44} -DBOOST_LIBRARYDIR=%{_libdir}/boost148 -DBOOST_INCLUDEDIR=/usr/include/boost148
make

%install
make install DESTDIR=${RPM_BUILD_ROOT}
make install-html DESTDIR=${RPM_BUILD_ROOT}

%clean
rm -rf %{buildroot}

%post -n libdune-cornerpoint1 -p /sbin/ldconfig

%postun -n libdune-cornerpoint1 -p /sbin/ldconfig

%files
%doc COPYING README

%files doc
%{_docdir}/*

%files -n libdune-cornerpoint1
%defattr(-,root,root,-)
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_libdir}/dunecontrol/*
%{_libdir}/pkgconfig/*
%{_includedir}/*
%{_datadir}/cmake/*

%files bin
%{_bindir}/*
