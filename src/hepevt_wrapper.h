#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "HepMC/FourVector.h"
#include "HepMC/Data/SmartPointer.h"
#include "HepMC/GenEvent.h"
#include "HepMC/GenParticle.h"
#include "HepMC/GenVertex.h"
#include <map>
#include <utility>

template <typename RealType>
bool fill_genevent_from_hepevt(HepMC::GenEvent& evt,
                               pybind11::array_t<RealType> p_array, // N x 4
                               pybind11::array_t<RealType> m_array, // N
                               pybind11::array_t<RealType> v_array, // N x 4
                               pybind11::array_t<int> status_array, // N
                               pybind11::array_t<int> pid_array,    // N
                               pybind11::array_t<int> parents_array, // N x 2
                               pybind11::array_t<int> /* children_array */, // N x 2, not used
                               RealType momentum_scaling = 1,
                               RealType length_scaling = 1) {
    using namespace HepMC;
    namespace py = pybind11;

    // see https://stackoverflow.com/questions/610245/where-and-why-do-i-have-to-put-the-template-and-typename-keywords
    auto pa = p_array.template unchecked<2>();
    auto va = v_array.template unchecked<2>();
    auto ma = m_array.template unchecked<1>();
    auto sta = status_array.template unchecked<1>();
    auto pid = pid_array.template unchecked<1>();
    auto par = parents_array.template unchecked<2>();
    // auto chi = children_array.template unchecked<2>();

    const int nentries = pa.shape(0);
    evt.set_event_number(nentries);

    /*
        first pass: add all particles to the event
    */
    for (int i = 0; i < nentries; ++i) {
        GenParticlePtr p = std::make_shared<GenParticle>();
        p->set_momentum(FourVector(pa(i, 0) * momentum_scaling,
                                   pa(i, 1) * momentum_scaling,
                                   pa(i, 2) * momentum_scaling,
                                   pa(i, 3) * momentum_scaling));
        p->set_status(sta(i));
        p->set_pid(pid(i));
        p->set_generated_mass(ma(i) * momentum_scaling);
        evt.add_particle(p);
    }

    /*
        second pass: add all vertices and connect topology

        Production vertices are duplicated for each outgoing particle in
        HEPEVT. Common production vertices are identified by having the same
        parents.

        Production vertices for particles without parents are not added.

        Children information is redundant and not used here, but could be used
        in Debug mode to check consistency of the event topology (TODO).
    */
    std::map<std::pair<int, int>, GenVertexPtr> vertex_map;
    for (int i = 0; i < nentries; ++i) {
        // HEPEVT uses Fortran style indices, convert to C++ style
        const auto parents = std::make_pair(par(i, 0)-1, par(i, 1)-1);
        // skip particles without parents
        if (parents.first == 0 || parents.second == 0 ||
            parents.first > parents.second)
            continue;
        // get unique production vertex for each particles with parents
        auto vi = vertex_map.find(parents);
        if (vi == vertex_map.end()) {
            GenVertexPtr v = std::make_shared<GenVertex>();
            v->set_position(FourVector(va(i, 0) * length_scaling,
                                       va(i, 1) * length_scaling,
                                       va(i, 2) * length_scaling,
                                       va(i, 3) * length_scaling));
            for (auto j = parents.first; j <= parents.second; ++j)
                v->add_particle_in(evt.particles()[j]);
            evt.add_vertex(v);
            vi = vertex_map.insert(std::make_pair(parents,v)).first;
        }
        vi->second->add_particle_out(evt.particles()[i]);
    }

    return true;
}
