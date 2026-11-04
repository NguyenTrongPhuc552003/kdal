# Homebrew formula for KDAL
# Install: brew install --build-from-source packaging/homebrew/kdal.rb
# Or from a tap: brew install kdal-project/tap/kdal
#
# This formula builds kdalc, kdality, libkdalc, and installs the
# standard library headers and CMake package files.

class Kdal < Formula
  desc "Kernel Device Abstraction Layer - compiler and toolchain"
  homepage "https://github.com/NguyenTrongPhuc552003/kdal"
  url "https://github.com/NguyenTrongPhuc552003/kdal/archive/refs/tags/v0.1.0.tar.gz"
  sha256 ""  # Populated by release automation
  license "GPL-3.0-or-later"
  head "https://github.com/NguyenTrongPhuc552003/kdal.git", branch: "main"

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build/release",
           "-DCMAKE_BUILD_TYPE=Release",
           *std_cmake_args
    system "cmake", "--build", "build/release"
    system "cmake", "--install", "build/release"
  end

  test do
    assert_match "kdality 0.1.0", shell_output("#{bin}/kdality version")
    assert_match "KDAL compiler", shell_output("#{bin}/kdalc -v")

    # Verify stdlib installed
    assert_predicate share/"kdal/stdlib/uart.kdh", :exist?
    assert_predicate share/"kdal/stdlib/gpio.kdh", :exist?
  end
end
