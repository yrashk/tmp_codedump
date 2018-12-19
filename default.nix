{ 
  pkgs ? import <nixpkgs> {}
}:

let c_binutils = pkgs.fetchurl {
  url = "http://ftp.gnu.org/gnu/binutils/binutils-2.25.1.tar.bz2";
  sha256 = "08lzmhidzc16af1zbx34f8cy4z7mzrswpdbhrb8shy3xxpflmcdm";
}; in
let c_expat = pkgs.fetchurl {
  url = "http://downloads.sourceforge.net/project/expat/expat/2.1.0/expat-2.1.0.tar.gz";
  sha256 = "11pblz61zyxh68s5pdcbhc30ha1b2vfjd83aiwfg4vc15x3hadw2";
}; in
let c_gmp = pkgs.fetchurl {
  url = "https://gmplib.org/download/gmp/gmp-6.0.0a.tar.xz";
  sha256 = "0r5pp27cy7ch3dg5v0rsny8bib1zfvrza6027g2mp5f6v8pd6mli";
}; in
let c_mprf = pkgs.fetchurl {
  url = "https://ftp.gnu.org/gnu/mpfr/mpfr-3.1.3.tar.xz";
  sha256 = "05jaa5z78lvrayld09nyr0v27c1m5dm9l7kr85v2bj4jv65s0db8";
}; in
let c_isl = pkgs.fetchurl {
  # Unavailable right now
  # url = "http://isl.gforge.inria.fr/isl-0.14.tar.xz";
  url = "ftp://ftp.cs.mun.ca/pub/mirror/gentoo/distfiles/isl-0.14.tar.xz";
  sha256 = "00zz0dcxvbna2fqqqv37sqlkqpffb2js47q7qy7p184xh414y15i";
}; in
let c_mpc = pkgs.fetchurl {
  url = "http://ftp.gnu.org/gnu/mpc/mpc-1.0.3.tar.gz";
  sha256 = "1hzci2zrrd7v3g1jk35qindq05hbl0bhjcyyisq9z209xb3fqzb1";
}; in
let c_ncurses = pkgs.fetchurl {
  url = "http://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.0.tar.gz";
  sha256 = "0q3jck7lna77z5r42f13c4xglc7azd19pxfrjrpgp2yf615w4lgm";
}; in
let c_gcc = pkgs.fetchurl {
  url = "http://ftp.gnu.org/gnu/gcc/gcc-5.2.0/gcc-5.2.0.tar.bz2";
  sha256 = "1bccp8a106xwz3wkixn65ngxif112vn90qf95m6lzpgpnl25p0sz";
}; in
let c_newlib = pkgs.fetchurl {
  url = "ftp://ftp.cs.mun.ca/pub/mirror/gentoo/distfiles/newlib-2.2.0.tar.gz";
  sha256 = "1gimncxzq663l4gp8zd89ynfzhk2q802mcaiyjpr2xbkn1ix5bgq";
}; in
let c_gdb = pkgs.fetchurl {
  url = "ftp://ftp.ntua.gr/pub/gnu/gdb/gdb-7.10.tar.gz";
  sha256 = "1r9w71n2jw4qx84ig67qrvimbddgx80r9v4h85aac5vrddnhwsah";
}; in
let crosstool = pkgs.stdenv.mkDerivation {
  name = "crosstool-ng";

  src = pkgs.fetchgit {
    url = "https://github.com/espressif/crosstool-NG";
    branchName = "xtensa-1.22.x";
    leaveDotGit = true;
    sha256 = "1civxplgrhcjv9w80vq4j1xma789xsd87in7djsqyvalnjf96xmc";
  };

  nativeBuildInputs = with pkgs; [ autoconf gperf bison flex help2man libtool
                             automake ncurses python file texinfo wget gcc7
                             git which ];

  configurePhase = ''
    ./bootstrap
    ./configure --enable-local
    make install
  '';

  buildPhase = ''
    # Pop `format` form hardening
    export NIX_HARDENING_ENABLE="fortify stackprotector pic strictoverflow relro bindnow" 
    # It will complain if it is set
    unset LD_LIBRARY_PATH
    mkdir -p .tarballs
    export CT_FORBID_DOWNLOAD=y
    ln -s ${c_binutils} .tarballs/binutils-2.25.1.tar.bz2
    ln -s ${c_expat} .tarballs/expat-2.1.0.tar.gz
    ln -s ${c_gmp} .tarballs/gmp-6.0.0a.tar.xz
    ln -s ${c_mprf} .tarballs/mpfr-3.1.3.tar.xz
    ln -s ${c_isl} .tarballs/isl-0.14.tar.xz
    ln -s ${c_mpc} .tarballs/mpc-1.0.3.tar.gz
    ln -s ${c_ncurses} .tarballs/ncurses-6.0.tar.gz
    ln -s ${c_gcc} .tarballs/gcc-5.2.0.tar.bz2
    ln -s ${c_newlib} .tarballs/newlib-2.2.0.tar.gz
    ln -s ${c_gdb} .tarballs/gdb-7.10.tar.gz
    ./ct-ng xtensa-esp32-elf 
    echo CT_LOCAL_TARBALLS_DIR=$(pwd)/.tarballs >> .config
    ./ct-ng build || { cat build.log; exit 1; }
  '';

  installPhase = ''
    mkdir -p $out
    cp -r builds/xtensa-esp32-elf/* $out/
  '';

}; in
let idf = pkgs.stdenv.mkDerivation {
  name = "idf";
  src = pkgs.fetchgit {
      url = "https://github.com/espressif/esp-idf";
      rev = "v3.3-beta1";
      sha256 = "1i5723ilwr3r7mawd8w0g897sml3d447308pfyjhg7x1am2ayvfp";
      fetchSubmodules = true;
      leaveDotGit = true;
    };

  propagatedBuildInputs = (with pkgs; [ ncurses flex bison gperf pkgconfig git python ])
                       ++ (with pkgs.pythonPackages; [ pyserial future cryptography pyparsing ]);

  installPhase = ''
    mkdir -p $out
    cp -r * $out/
  '';

 }; in
pkgs.stdenv.mkDerivation rec {
  name = "seclet-shell";

  buildInputs = [ crosstool idf ];

  shellHook = ''
    export IDF_PATH=$(mktemp -d)
    # Clone a copy
    cp --no-preserve=ownership -r ${idf}/* $IDF_PATH/
    chmod -R u+w $IDF_PATH/
  '';

  dontInstall = true;
}

