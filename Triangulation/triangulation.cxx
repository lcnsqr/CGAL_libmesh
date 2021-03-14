// libMesh
#include "libmesh/libmesh.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/elem.h"
#include "libmesh/cell_tet4.h"
#include "libmesh/node.h"
#include "libmesh/mesh_tools.h"
#include "libmesh/boundary_info.h"
// Formato ExodusII
#include "libmesh/exodusII_io.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Delaunay_triangulation_3.h>
#include <CGAL/Delaunay_triangulation_cell_base_3.h>
#include <CGAL/Triangulation_vertex_base_with_info_3.h>

#include <vector>


#define RAND ((double)(rand() >> 1)/((RAND_MAX >> 1) + 1))

typedef CGAL::Exact_predicates_inexact_constructions_kernel         K;
// O info do ponto armazena o id de um nó (se houver) na casca (libmesh) original
typedef CGAL::Triangulation_vertex_base_with_info_3<libMesh::dof_id_type, K>    Vb;
typedef CGAL::Delaunay_triangulation_cell_base_3<K>                 Cb;
typedef CGAL::Triangulation_data_structure_3<Vb, Cb>                Tds;
//Use the Fast_location tag. Default or Compact_location works too.
typedef CGAL::Delaunay_triangulation_3<K, Tds, CGAL::Fast_location> Delaunay;
//typedef Delaunay::Point                                             Point;

// libMesh namespace
using namespace libMesh;

int main(int argc, char **argv) {

  // Iniciar libMesh e bibliotecas subjacentes (PETSc)
  LibMeshInit init (argc, argv);

  // Instanciar uma nova malha
  Mesh mesh_hull(init.comm());

  // Dimensão da malha
  int dim = 3;

  // Gerar uma malha uniforme num cubo de lado 2 e aresta com 15 elementos
  Real halfside = 1.;
  MeshTools::Generation::build_cube (mesh_hull, 15, 15, 15, -halfside, halfside, -halfside, halfside, -halfside, halfside, TET4);
  //mesh.read ("3D.off");

  // Contorno do casco, assumindo que existem nós associados a contornos
  BoundaryInfo & boundary_info_hull = mesh_hull.get_boundary_info();

  // Exibir informações da malha
  mesh_hull.print_info();
  boundary_info_hull.print_summary();

  // Vetor de tuplas dof_id/boundary_id referente a nós e contornos
  typedef std::vector<std::tuple<dof_id_type,boundary_id_type>> nodes_boundaries_type;
  nodes_boundaries_type nodes_boundaries;
  nodes_boundaries = boundary_info_hull.build_node_list();

  // Mapa com cada nó e seus contornos correspondentes
  std::map<dof_id_type, std::vector<boundary_id_type>> hull_node_boundaries;

  // Preencher o mapa 
  for (nodes_boundaries_type::iterator nb_it = nodes_boundaries.begin(); 
    nb_it != nodes_boundaries.end();
    ++nb_it)
    {
       hull_node_boundaries[std::get<0>(*nb_it)].push_back(std::get<1>(*nb_it));
    }

  // [CGAL] Instanciar uma triangulação Delaunay 3D vazia
  Delaunay T;

  // [CGAL] Nós inseridos na malha CGAL
  std::map<dof_id_type, bool> cgal_added_nodes;

  // Inserir nós de contorno na malha CGAL
  for (auto & elem : mesh_hull.element_ptr_range())
    for (auto s : elem->side_index_range())
      if (elem->neighbor_ptr(s) == nullptr)
      {
        // Em cada face de elemento de contorno, gerar um mapa 
        // de contornos e seus nós correspondentes
        std::map<boundary_id_type, std::vector<dof_id_type>> hull_boundary_nodes;

        // Face do elemento
        std::unique_ptr<Elem> side = elem->side_ptr(s);

        // Percorrer cada nó na face
        for (auto n : side->node_index_range())
        {
          // Identificar boundary IDs associados ao nó
          for (std::vector<boundary_id_type>::iterator bid_it = hull_node_boundaries[side->node_id(n)].begin();
               bid_it != hull_node_boundaries[side->node_id(n)].end();
               ++bid_it)
          {
            // Incluir nó no vetor correspondente a cada um de seus boundary ID
            hull_boundary_nodes[*bid_it].push_back(side->node_id(n));
            // Se algum boundary ID totalizar o número de
            // nós total da face, a face está no contorno
            if ( hull_boundary_nodes[*bid_it].size() == side->n_nodes() )
            {
              // Todos os nós deste lado estão no contorno *bid_it
              for (std::vector<dof_id_type>::iterator node_it = hull_boundary_nodes[*bid_it].begin();
                   node_it != hull_boundary_nodes[*bid_it].end();
                   ++node_it)
              {

                // Ponteiro para o nó
                const Node * nptr = mesh_hull.node_ptr(*node_it);

                // Coordenadas do nó
                libMesh::Point p = *nptr;

                // Inserir ponto na triangulação
                if ( ! cgal_added_nodes[*node_it] )
                {
                  Delaunay::Vertex_handle vh = T.insert(Delaunay::Point(p(0),p(1),p(2)));
                  // Id do nó na malha libmesh original + 1 (evita usar 0 para identificação)
                  vh->info() = *node_it + 1;
                  // Marcar nó como incluído
                  cgal_added_nodes[*node_it] = true;
                }

              }
            }
          }
        }
      }


  // Incluir uns pontinhos malandros
  for (int i = 0 ; i < 3000; i++)
    T.insert(Delaunay::Point(-1. + 2.*RAND,-1. + 2.*RAND,-1. + 2.*RAND));

  // Salvar malha original
  mesh_hull.write("antes.e");

  assert(T.is_valid());

	// Nova malha libmesh
  Mesh mesh(init.comm());
	mesh.set_mesh_dimension(dim);
	mesh.set_spatial_dimension(dim);

  // Nós inseridos na nova malha libmesh
  std::map<Delaunay::Vertex_handle, bool> mesh_added_nodes;

  // Mapa vertex_handle -> novo node id
  std::map<Delaunay::Vertex_handle, dof_id_type> cgal_vertex_to_new_node;

  // Mapa com cada nó da nova malha libmesh e seus contornos correspondentes
  std::map<dof_id_type, std::vector<boundary_id_type>> node_boundaries;

  // Número sequencial de identificação do novo elemento
	unsigned int elem_id = 0;

  // [CGAL] Percorrer todas as células da malha gerada
  for (Delaunay::Finite_cells_iterator cit = T.finite_cells_begin(); cit != T.finite_cells_end(); cit++)
  {

		// Novo elemento na malha libmesh nova
		libMesh::Elem * elem = new libMesh::Tet4;
		elem->set_id(elem_id++);

		// Percorre os 4 pontos do tetraedro e insere os nós correspondentes
		for (int n = 0; n < 4; n++)
		{

      // Nó a inserir na nova malha
      dof_id_type new_node_id;
      Node *new_node;

      if ( ! mesh_added_nodes[cit->vertex(n)] )
      {
        // Ausente na malha nova, inserir nó
				new_node = mesh.add_point(libMesh::Point(
					cit->vertex(n)->point().x(), 
					cit->vertex(n)->point().y(), 
					cit->vertex(n)->point().z()
				));
        new_node_id = new_node->id();
        // Marcar como inserido no mapa
        mesh_added_nodes[cit->vertex(n)] = true;
        // Associar ao nó inserido
        cgal_vertex_to_new_node[cit->vertex(n)] = new_node_id;

        // Se vertex refere-se a um nó de contorno, recuperar seus boundary IDs
        if ( cit->vertex(n)->info() > 0 )
        {
          // Associar novo node -> boundary ids
          node_boundaries[new_node_id] = hull_node_boundaries[cit->vertex(n)->info() - 1];
        }

      }
      else
      {
        // Nó já foi inserido
        new_node_id = cgal_vertex_to_new_node[cit->vertex(n)];
      }

			// Definir n-ésimo nó do elemento
			elem->set_node(n) = mesh.node_ptr(new_node_id);
		}

		// Incluir elemento na malha nova
		elem = mesh.add_elem(elem);

  }

  // Contorno a definir
  BoundaryInfo & boundary_info = mesh.get_boundary_info();

  // Construir Boundary info da malha nova
  for (auto & elem : mesh.element_ptr_range())
    for (auto s : elem->side_index_range())
      if (elem->neighbor_ptr(s) == nullptr)
      {
        // Em cada face de elemento de contorno, gerar um mapa 
        // de contornos e seus nós correspondentes
        std::map<boundary_id_type, std::vector<dof_id_type>> boundary_nodes;

        // Face do elemento
        std::unique_ptr<Elem> side = elem->side_ptr(s);

        // Percorrer cada nó na face
        for (auto n : side->node_index_range())
        {
          // Identificar boundary IDs associados ao nó
          for (std::vector<boundary_id_type>::iterator bid_it = node_boundaries[side->node_id(n)].begin();
               bid_it != node_boundaries[side->node_id(n)].end();
               ++bid_it)
          {
            // Incluir nó no contador da boundary ID
            boundary_nodes[*bid_it].push_back(side->node_id(n));
            // Se algum boundary ID totalizar o número de
            // nós total da face, a face está no contorno
            if ( boundary_nodes[*bid_it].size() == side->n_nodes() )
            {
              // Todos os nós deste lado estão no contorno *bid_it
              boundary_info.add_side(elem, s, *bid_it);
            }
          }
        }
      }

  mesh.prepare_for_use();
	// Exibir informações da malha reconstruída
  mesh.print_info();
  boundary_info.print_summary();

  // Salvar malha gerada
  mesh.write("depois.e");


  return 0;
}
