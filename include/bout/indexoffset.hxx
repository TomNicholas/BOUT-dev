#ifndef __INDEXOFFSET_H__
#define __INDEXOFFSET_H__

#include "bout/mesh.hxx"
#include "bout/region.hxx"

template <typename T = Ind3D>
struct IndexOffset {

  static_assert(std::is_base_of<Ind2D, T>::value || std::is_base_of<Ind3D, T>::value,
                "IndexOffset must be templated with either Ind2D or Ind3D");

  Mesh &mesh;
  const int nx, ny, nz;

  IndexOffset(Mesh &mesh)
      : mesh(mesh), nx(mesh.LocalNx), ny(mesh.LocalNy), nz(mesh.LocalNz) {}

  /// Convenience functions for converting to (x, y, z)
  int x(T index) const { return (index.ind / nz) / ny; }
  int y(T index) const { return (index.ind / nz) % ny; }
  int z(T index) const { return (index.ind % nz); }

  const inline T xp(T index, int i = 1) const { return index + (i * ny * nz); }
  /// The index one point -1 in x
  const inline T xm(T index, int i = 1) const { return index - (i * ny * nz); }
  /// The index one point +1 in y
  const inline T yp(T index, int i = 1) const { return index + (i * nz); }
  /// The index one point -1 in y
  const inline T ym(T index, int i = 1) const { return index - (i * nz); }
  /// The index one point +1 in z. Wraps around zend to zstart
  const inline T zp(T index, int i = 1) const {
    return (index + i) % nz == 0 ? index - nz + i : index + i;
  }
  /// The index one point -1 in z. Wraps around zstart to zend
  const inline T zm(T index, int i = 1) const {
    return index % nz == 0 ? index + nz - i : index - i;
  }
  // and for 2 cells
  const inline T xpp(T index) const { return xp(index, 2); }
  const inline T xmm(T index) const { return xm(index, 2); }
  const inline T ypp(T index) const { return yp(index, 2); }
  const inline T ymm(T index) const { return ym(index, 2); }
  const inline T zpp(T index) const { return zp(index, 2); }
  const inline T zmm(T index) const { return zm(index, 2); }
};

#endif // __INDEXOFFSET_H__
