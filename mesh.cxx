// C++ include files that we need
#include <iostream>
#include <algorithm>
#include <math.h>
#include <set>
#include <unordered_set>
#include <vector>

// Basic include file needed for the mesh functionality.
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/elem.h"
#include "libmesh/node.h"
#include "libmesh/mesh_tools.h"
#include "libmesh/boundary_info.h"

// Formato ExodusII
#include "libmesh/exodusII_io.h"

#include "refine3d.hxx"

// Bring in everything from the libMesh namespace
using namespace libMesh;

// Begin the main program.
int main (int argc, char ** argv)
{
  // Initialize libMesh and any dependent libraries, like in example 2.
  LibMeshInit init (argc, argv);

  // Dimension
  int dim = 3;

  // Create a mesh with user-defined dimension.
  // Number of elements
  int ps = 1;

  // Create a mesh, with dimension to be overridden later, distributed
  // across the default MPI communicator.
  Mesh mesh(init.comm());

  // Use the MeshTools::Generation mesh generator to create a uniform
  // grid on the square [-1,1]^D.  We instruct the mesh generator
  // to build a mesh of TET4 elements in 3D.

  Real halfwidth = 1.;
  Real halfheight = 1.;

  MeshTools::Generation::build_cube (mesh,
                                     ps,
                                     (dim>1) ? ps : 0,
                                     (dim>2) ? ps : 0,
                                     -1., 1.,
                                     -halfwidth, halfwidth,
                                     -halfheight, halfheight,
                                     TET4);

  // Print information about the mesh to the screen.
  mesh.print_info();

  // Todos os nós que estão no contorno
  std::unordered_set<dof_id_type> boundary_nodes;
  boundary_nodes = libMesh::MeshTools::find_boundary_nodes(mesh);

  // Vetor com os triângulos do casco
	std::vector<refine3d::Triangulo<refine3d::Ponto<double>>> casco;

  // Passar por todos os elementos ativos
  MeshBase::const_element_iterator el = mesh.active_elements_begin(), end_el = mesh.active_elements_end();
  for ( ; el != end_el ; el++ )
	{
		const Elem * elem = *el;

    // Ignorar se não for um elemento de contorno
    if ( ! elem->on_boundary() ) continue;

    libMesh::out << "Face externa do elemento de contorno " << elem->id() << ":" << std::endl;

    // Os três vértices de um elemento de contorno
    std::array<refine3d::Ponto<double>, 3> vertices;

    // Passar por todos os nós do elemento
    uint v = 0;
    for (uint nnid=0; nnid < elem->n_nodes(); nnid++)
		{
      // Obter o ID do nó
      const Node * nptr = elem->node_ptr(nnid);
      uint nid = nptr->id();

      // Verificar se nó está na face externa do elemento
      if (boundary_nodes.find(nid) != boundary_nodes.end()){

        // Coordenadas do nó de contorno
        Point p = *nptr;
        libMesh::out << "Nó de contorno (" << nid << "): " << p(0) << ", " << p(1) << ", " << p(2) << std::endl;

        // Atualizar vértice correspondente
        vertices[v].pos[0] = p(0);
        vertices[v].pos[1] = p(1);
        vertices[v].pos[2] = p(2);
        v++;

      }

		}

    // Adicionar triângulo de contorno ao vetor do casco
    casco.push_back(refine3d::Triangulo<refine3d::Ponto<double>>(vertices[0],vertices[1],vertices[2]));
	}

  libMesh::out << "Salvando cube.e" << std::endl;
  mesh.write("cube.e");

  libMesh::out << "Salvando cube_refined.mesh" << std::endl;
	refine3d::mesh_hull(casco);

  // All done.
  return 0;
}