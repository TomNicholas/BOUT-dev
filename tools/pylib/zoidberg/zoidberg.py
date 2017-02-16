from __future__ import division
from builtins import object
from past.utils import old_div

import numpy as np
from boututils import datafile as bdata

# PyEVTK might be called pyevtk or evtk, depending on where it was
# installed from
have_evtk = True
try:
    from pyevtk.hl import gridToVTK
except ImportError:
    try:
        from evtk.hl import gridToVTK
    except ImportError:
        have_evtk = False

import matplotlib.pyplot as plt

from . import grid
from . import field
from . import fieldtracer
from .progress import update_progress

def make_maps(grid, magnetic_field, quiet=False, **kwargs):
    """Make the forward and backward FCI maps

    Inputs
    ------
    grid           - Grid generated by Zoidberg
    magnetic_field - Zoidberg magnetic field object
    quiet          - Don't display progress bar [False]
    **kwargs       - Arguments for field line tracing, etc.
    """

    nx, ny, nz = (grid.nx, grid.ny, grid.nz)

    # Arrays to store X index at end of field-line
    # starting from (x,y,z) and going forward in toroidal angle (y)
    forward_xt_prime = np.zeros( (nx, ny, nz) )
    forward_zt_prime = np.zeros( (nx, ny, nz) )

    # Same but going backwards in toroidal angle
    backward_xt_prime = np.zeros( (nx, ny, nz) )
    backward_zt_prime = np.zeros( (nx, ny, nz) )

    x2d, z2d = np.meshgrid(grid.xarray, grid.zarray, indexing='ij')
    field_tracer = fieldtracer.FieldTracer(magnetic_field)

    try:
        rtol = kwargs["rtol"]
    except KeyError:
        rtol = None

    # TODO: if axisymmetric, don't loop, do one slice and copy
    for j in range(ny):
        if not quiet:
            update_progress(float(j)/float(ny-1), **kwargs)

        # Go forwards from yarray[j] by an angle delta_y
        y_coords = [grid.yarray[j], grid.yarray[j]+grid.delta_y]
        # We only want the end point, as [0,...] is the initial position
        coord = field_tracer.follow_field_lines(x2d, z2d, y_coords, rtol=rtol)[1,...]
        forward_xt_prime[:,j,:] = (grid.MXG - 0.5) + coord[:,:,0] / grid.delta_x # X index
        forward_zt_prime[:,j,:] = coord[:,:,1] / grid.delta_z # Z index

        # Go backwards from yarray[j] by an angle -delta_y
        y_coords = [grid.yarray[j], grid.yarray[j]-grid.delta_y]
        # We only want the end point, as [0,...] is the initial position
        coord = field_tracer.follow_field_lines(x2d, z2d, y_coords, rtol=rtol)[1,...]
        backward_xt_prime[:,j,:] = (grid.MXG - 0.5) + coord[:,:,0] / grid.delta_x # X index
        backward_zt_prime[:,j,:] = coord[:,:,1] / grid.delta_z # Z index

    maps = {
        'forward_xt_prime' : forward_xt_prime,
        'forward_zt_prime' : forward_zt_prime,
        'backward_xt_prime' : backward_xt_prime,
        'backward_zt_prime' : backward_zt_prime
    }

    return maps

def write_maps(grid, magnetic_field, maps, gridfile='fci.grid.nc', legacy=False):
    """Write FCI maps to BOUT++ grid file

    Inputs
    ------
    grid           - Grid generated by Zoidberg
    magnetic_field - Zoidberg magnetic field object
    maps           - Dictionary of FCI maps
    gridfile       - Output filename
    legacy         - If true, write FCI maps using FFTs
    """

    nx, ny, nz = (grid.nx, grid.ny, grid.nz)
    xarray, yarray, zarray = (grid.xarray, grid.yarray, grid.zarray)

    g_22 = np.ones((nx,ny))# + 1./grid.Rmaj(grid.xarray,grid.yarray)**2

    with bdata.DataFile(gridfile, write=True, create=True) as f:
        ixseps = nx+1
        f.write('nx', grid.nx)
        f.write('ny', grid.ny)
        if not legacy:
            # Legacy files don't need nz
            f.write('nz', grid.nz)

        f.write("dx", grid.delta_x)
        f.write("dy", grid.delta_y)
        f.write("dz", grid.delta_z)

        f.write("ixseps1",ixseps)
        f.write("ixseps2",ixseps)

        f.write("g_22", g_22)

        f.write("Bxy", magnetic_field.b_mag[:,:,0])
        f.write("bx", magnetic_field.bx)
        f.write("bz", magnetic_field.bz)

        # Legacy grid files need to FFT 3D arrays
        if legacy:
            from boutdata.input import transform3D
            f.write('forward_xt_prime',  transform3D(maps['forward_xt_prime']))
            f.write('forward_zt_prime',  transform3D(maps['forward_zt_prime']))

            f.write('backward_xt_prime', transform3D(maps['backward_xt_prime']))
            f.write('backward_zt_prime', transform3D(maps['backward_zt_prime']))
        else:
            f.write('forward_xt_prime',  maps['forward_xt_prime'])
            f.write('forward_zt_prime',  maps['forward_zt_prime'])

            f.write('backward_xt_prime', maps['backward_xt_prime'])
            f.write('backward_zt_prime', maps['backward_zt_prime'])

def write_Bfield_to_vtk(grid, magnetic_field, scale=5, vtkfile="fci_zoidberg", psi=True):
    """Write the magnetic field to a VTK file

    Inputs
    ------
    grid           - Grid generated by Zoidberg
    magnetic_field - Zoidberg magnetic field object
    scale          - Factor to scale x, z dimensions by [5]
    vtkfile        - Output filename without extension ["fci_zoidberg"]

    Returns
    -------
    path           - Full path to vtkfile
    """

    point_data = {'B' : (magnetic_field.bx*scale,
                         magnetic_field.by,
                         magnetic_field.bz*scale)}

    if psi:
        psi = make_surfaces(grid, magnetic_field)
        point_data['psi'] = psi

    path = gridToVTK(vtkfile,
                     grid.xarray*scale,
                     grid.yarray,
                     grid.zarray*scale,
                     pointData=point_data)

    return path

def fci_to_vtk(infile, outfile, scale=5):

    if not have_evtk:
        return

    with bdata.DataFile(infile, write=False, create=False) as f:
        dx = f.read('dx')
        dy = f.read('dy')

        bx = f.read('bx')
        by = np.ones(bx.shape)
        bz = f.read('bz')
        if bx is None:
            xt_prime = f.read('forward_xt_prime')
            zt_prime = f.read('forward_zt_prime')
            array_indices = indices(xt_prime.shape)
            bx = xt_prime - array_indices[0,...]
            by = by * dy
            bz = zt_prime - array_indices[2,...]

        nx, ny, nz = bx.shape
        dz = nx*dx / nz

    x = np.linspace(0, nx*dx, nx)
    y = np.linspace(0, ny*dy, ny, endpoint=False)
    z = np.linspace(0, nz*dz, nz, endpoint=False)

    gridToVTK(outfile, x*scale, y, z*scale, pointData={'B' : (bx*scale, by, bz*scale)})

def make_surfaces(grid, magnetic_field, nsurfaces=10, revs=100):
    """Essentially interpolate a poincare plot onto the grid mesh

    Inputs
    ------
    grid           - Grid generated by Zoidberg
    magnetic_field - Zoidberg magnetic field object
    nsurfaces      - Number of surfaces to interpolate to [10]
    revs           - Number of points on each surface [100]

    Returns
    -------
    surfaces - Array of psuedo-psi on the grid mesh

    """

    from scipy.interpolate import griddata

    # initial x, z points in surface
    xpos = grid.xcentre + np.linspace(0, 0.5*np.max(grid.xarray),
                                           nsurfaces)
    zpos = grid.zcentre

    phi_values = grid.yarray[:]
    # Extend the domain from [0,grid.Ly] to [0,revs*grid.Ly]
    for n in np.arange(1, revs):
        phi_values = np.append(phi_values, n*grid.Ly + phi_values[:grid.ny])

    # Get field line tracer and trace out surfaces
    tracer = fieldtracer.FieldTracer(magnetic_field)
    points = tracer.follow_field_lines(xpos, zpos, phi_values)

    # Reshape to be easier to work with
    points = points.reshape((revs, grid.ny, nsurfaces, 2))

    # Arbitarily number the surfaces from 0 to 1
    psi_points = np.zeros((revs, grid.ny, nsurfaces))
    for surf in range(nsurfaces):
        psi_points[:,:,surf] = float(surf)/float(nsurfaces-1)

    x_2d, z_2d = np.meshgrid(grid.xarray, grid.zarray, indexing='ij')

    psi = np.zeros_like(grid.x_3d)
    for y_slice in range(grid.ny):
        points_2d = np.column_stack((points[:,y_slice,:,0].flatten(),
                                     points[:,y_slice,:,1].flatten()))
        psi[:,y_slice,:] = griddata(points_2d, psi_points[:,y_slice,:].flatten(),
                                    (x_2d, z_2d), method='linear', fill_value=1)

    return psi

def upscale(field, maps, upscale_factor=4, quiet=True):
    """Increase the resolution in y of field along the FCI maps.

    First, interpolate onto the (forward) field line end points, as in
    normal FCI technique. Then interpolate between start and end
    points. We also need to interpolate the xt_primes and
    zt_primes. This gives a cloud of points along the field lines,
    which we can finally interpolate back onto a regular grid.

    Inputs
    ------
    field          - 3D field to be upscaled
    maps           - Zoidberg field line maps
    upscale_factor - Factor to increase resolution by [4]
    quiet          - Don't show progress bar [True]

    Returns
    -------
    Field with y-resolution increased *upscale_factor* times. Shape is
    (nx, upscale_factor*ny, nz).

    """

    from scipy.ndimage.interpolation import map_coordinates
    from scipy.interpolate import griddata

    xt_prime = maps["forward_xt_prime"]
    zt_prime = maps["forward_zt_prime"]

    # The field should be the same shape as the grid
    if field.shape != xt_prime.shape:
        try:
            field = field.reshape(xt_prime.T.shape).T
        except ValueError:
            raise ValueError("Field, {}, must be same shape as grid, {}"
                             .format(field.shape, xt_prime.shape))

    # Get the shape of the grid
    nx, ny, nz = xt_prime.shape
    index_coords = np.mgrid[0:nx, 0:ny, 0:nz]

    # We use the forward maps, so get the y-index of the *next* y-slice
    yup_3d = index_coords[1,...] + 1
    yup_3d[:,-1,:] = 0

    # Index space coordinates of the field line end points
    end_points = np.array([xt_prime, yup_3d, zt_prime])

    # Interpolation of the field at the end points
    field_prime = map_coordinates(field, end_points)

    # This is a 4D array where the first dimension is the start/end of
    # the field line
    field_aligned = np.array([field, field_prime])

    # x, z coords at start/end of field line
    x_start_end = np.array([index_coords[0,...], xt_prime])
    z_start_end = np.array([index_coords[2,...], zt_prime])

    # Parametric points along the field line
    midpoints = np.linspace(0, 1, upscale_factor, endpoint=False)
    # Need to make this 4D as well
    new_points = np.tile(midpoints[:,np.newaxis,np.newaxis,np.newaxis], [nx, ny, nz])

    # Index space coordinates of our upscaled field
    index_4d = np.mgrid[0:upscale_factor,0:nx,0:ny,0:nz]
    hires_points = np.array([new_points, index_4d[1,...], index_4d[2,...], index_4d[3,...]])

    # Upscale the field
    hires_field = map_coordinates(field_aligned, hires_points)

    # Linearly interpolate the x, z coordinates of the field lines
    hires_x = map_coordinates(x_start_end, hires_points)
    hires_z = map_coordinates(z_start_end, hires_points)

    def twizzle(array):
        """Transpose and reshape the output of map_coordinates to
        be 3D
        """
        return array.transpose((1, 2, 0, 3)).reshape((nx, upscale_factor*ny, nz))

    # Rearrange arrays to be 3D
    hires_field = twizzle(hires_field)
    hires_x = twizzle(hires_x)
    hires_z = twizzle(hires_z)

    # Interpolate from field line sections onto grid
    hires_grid_field = np.zeros( (nx, upscale_factor*ny, nz) )
    hires_index_coords = np.mgrid[0:nx, 0:ny:1./upscale_factor, 0:nz]
    grid_points = (hires_index_coords[0,:,0,:], hires_index_coords[2,:,0,:])

    def y_first(array):
        """Put the middle index first
        """
        return array.transpose((0, 2, 1))

    # The hires data is unstructed only in (x,z), interpolate onto
    # (x,z) grid for each y-slice individually
    for k, (x_points, z_points, f_slice) in enumerate(zip(y_first(hires_x).T, y_first(hires_z).T, y_first(hires_field).T)):
        points = np.column_stack((x_points.flat, z_points.flat))
        hires_grid_field[:,k,:] = griddata(points, f_slice.flat, grid_points,
                                           method='linear', fill_value=0.0)
        if not quiet:
            update_progress(float(k)/float(ny-1))

    return hires_grid_field
