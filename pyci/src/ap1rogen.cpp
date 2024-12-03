/* This file is part of PyCI.
 *
 * PyCI is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * PyCI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PyCI. If not, see <http://www.gnu.org/licenses/>. */

#include <pyci.h>
#include <unorderd_map>
#include <iostream>
namespace pyci {

// See apig.cpp for reference

// Constructor with Raw Pointers

// call to the base class constructor fullciwfn with the provided parameters
AP1roGeneralizedSenoObjective::AP1roGeneralizedSenoObjective(const SparseOp &op_, const NonSingletCI &wfn_,
                                 const std::size_t n_detcons_,
                                 const long *idx_detcons_,
                                 const double *val_detcons_,
                                 const std::size_t n_paramcons_,
                                 const long *idx_paramcons_,
                                 const double *val_paramcons_)
: Objective<NonSingletCI>::Objective(op_, wfn_, n_detcons_, idx_detcons_, val_detcons_, n_paramcons_, idx_paramcons_, val_paramcons_)
{
    init_overlap(wfn_); 
}
// call to initizlize the overlap related data

// Constructor with pybind11 objects
AP1roGeneralizedSenoObjective::AP1roGeneralizedSenoObjective(const SparseOp &op_, const NonSingletCI &wfn_,
                                 const pybind11::object idx_detcons_,
                                 const pybind11::object val_detcons_,
                                 const pybind11::object idx_paramcons_,
                                 const pybind11::object val_paramcons_)
: Objective<NonSingletCI>::Objective(op_, wfn_, idx_detcons_, val_detcons_, idx_paramcons_, val_paramcons_)
{
    init_overlap(wfn_);
}
// Copy Constructor
// obj is the constant reference to another object to be copied
AP1roGeneralizedSenoObjective::AP1roGeneralizedSenoObjective(const AP1roGeneralizedSenoObjective &obj)
: Objective<NonSingletCI>::Objective(obj), nrow(obj.nrow), ncol(obj.ncol),
  nexc_list(obj.nexc_list), hole_list(obj.hole_list), part_list(obj.part_list)
{
    return;
}

// Move constructor
// obj is the rvalue reference to another object to be moved
AP1roGeneralizedSenoObjective::AP1roGeneralizedSenoObjective(AP1roGeneralizedSenoObjective &&obj) noexcept
: Objective<NonSingletCI>::Objective(obj), nrow(std::exchange(obj.nrow, 0)), ncol(std::exchange(obj.ncol, 0)),
  nexc_list(std::move(obj.nexc_list)), hole_list(std::move(obj.hole_list)), part_list(std::move(obj.part_list))
{
    return;
}

void AP1roGeneralizedSenoObjective::generate_combinations(const std::vector<T>& elems, int k, std::vector<std::vector<T>>& result) {
    std::vector<bool> mask(elems.size());
    std::fill(mask.end() - k, mask.end() + k, true);
    do {
        std::vector<T> combination;
        for (std::size_t i = 0; i < elems.size(); ++i) {
            if (mask[i]) combination.push_back(elems[i]);
        }
        result.push_back(combination);
    } while (std::next_permutation(mask.begin(), mask.end()));
}


void AP1roGeneralizedSenoObjective::init_overlap(const NonSingletCI &wfn_)
{
    std::cout << "Inside init_overlap" << std::endl;
    // Initialize your class-specific variables here
    // init_Overlap objective for the AP1roGSDspin_sen-o 
    nparam = wfn_.nocc_up * (wfn_.nbasis - wfn_.nocc_up); //paired-doubles
    std::cout << "nparam (doubles): " << nparam << std::endl;
    nparam += wfn_.nocc * (2* wfn_.nbasis - wfn_.nocc); // beta singles
    std::cout << "nparam (doubles + S_alpha + S_beta): " << nparam << std::endl;
    // nrow = wfn_.nocc_up;
    // ncol = wfn_.nbasis - wfn_.nocc_up;
    // std::cout << "nrow: " << nrow << ", ncol: " << ncol << std::endl;
    // std::cout << "nconn: " << nconn << std::endl;

    ovlp.resize(wfn_.ndet);
    d_ovlp.resize(wfn_.ndet * nparam);

    // nexc_list.resize(nconn);
    // hole_list.resize(wfn_.nocc * nconn); //list of all holes 
    // part_list.resize(wfn_.nocc * nconn); //list of all particles

    std::size_t nword = (ulong)wfn_.nword;

    std::unordered_map<std::vector<ulong>, DetExcParamIndx> det_map;

    // Populate the hash map (assume wfn_.det_exc_param_indx is iterable)
    for (const auto& exc_info : wfn_.det_exc_param_indx) {
        det_map[exc_info.det] = exc_info; // Use exc_info.det as the key
    }

    for (std::size_t idet = 0; idet != nconn; ++idet)
    {
        std::vector<ulong> rdet(wfn_.nword);
        wfn_.fill_hartreefock_det(wfn_.nocc, &rdet[0]);

        std::cout << "After fill_hartreefock_det rdet:" << std::endl;
        print_vector("rdet", rdet);

        const ulong *det = wfn_.det_ptr(idet);

        std::vector<ulong> det_vector(det, det + nword);
        auto it = det_map.find(det_vector);
        if (it != det_map.end()) {
            std::cout << "Found det in det_map" << std::endl;
            std::cout << "Det: " << det_vector << std::endl;
            std::cout << "DetExcParamIndx: " << it->second << std::endl;
        } else {
            std::cout << "Det not found in det_map" << std::endl;
            DetExcParamIndx exc_info;
            ulong word, hword, pword;
            std::size_t h, p, nexc = 0;

            std::vector<std::size_t> holes;
            std::vector<std::size_t> particles;

            // Collect holes and particles
            for (std::size_t iword = 0; iword != nword; ++iword)
            {
                word = rdet[iword] ^ det[iword]; //str for excitation
                hword = word & rdet[iword]; //str for hole
                pword = word & det[iword]; //str for particle
                while(hword){
                    h = Ctz(hword);
                    p = Ctz(pword);
                    // hole_list[idet * wfn_.nocc_up + nexc] = h + iword * Size<ulong>();
                    // part_list[idet * wfn_.nocc_up + nexc] = p + iword * Size<ulong>() - wfn_.nocc_up;
                    
                    std::size_t hole_idx = h + iword * Size<ulong>();
                    std::size_t part_idx = p + iword * Size<ulong>() - wfn_.nocc_up;
                    
                    holes.push_back(hole_idx);
                    particles.push_back(part_idx);

                    hword &= ~(1UL << h);
                    pword &= ~(1UL << p);
                    std::cout << "hword" << hword << std::endl;
                    std::cout << "pword" << pword << std::endl;
                    std::cout << "nexc: " << nexc << std::endl;
                    std::cout << "hole_list: " << hole_idx << std::endl;
                    std::cout << "part_list: " << part_idx << std::endl;
                    ++nexc;
                }
            }
            nexc_list[idet] = nexc;

            std::vector<std::pair<std::size_t, std::size_t>> occ_pairs;
            for (std::size_t hole in holes) {
                std::size_t conjugate = hole + wfn_.nbasis / 2;
                if(std::find(holes.begin(), holes.end(), conjugate) != holes.end()) {
                    occ_pairs.push_back(std::make_pair(hole, conjugate));
                    // exc_info.pair_inds.push_back(wfn_.nvir_up * hole);
                }
            }

            std::vector<std::size_t, std::size_t> vir_pairs;
            for (std::size_t part in particles) {
                std::size_t conjugate = part + wfn_.nbasis / 2;
                if(std::find(particles.begin(), particles.end(), conjugate) != particles.end()) {
                    vir_pairs.push_back(std::make_pair(part, conjugate));
                    // exc_info.pair_inds.push_back(wfn_.nvir_up * part);
                }
            }

            for (const auto& pair : occ_pairs) {
               for (const auto& vir_pair : vir_pairs) {
                   exc_info.pair_inds.push_back(wfn_.nvir_up * pair.first + vir_pair.first);
                   exc_info.pair_inds.push_back(wfn_.nvir_up * pair.second + vir_pair.second);
               }
            }

            
        }
    }
}

void AP1roGeneralizedSenoObjective::overlap(const size_t ndet, const double *x, double *y)
{
    // x == parameters p_j
    // y == overlap vector σ_i
    std::size_t m, i, j, k, c;
    std::size_t *hlist, *plist;
    double rowsum, rowsumprod, out;
    
    for (std::size_t idet =0; idet != ndet; ++idet)
    {
        m = nexc_list[idet];
        if (m == 0) {
            y[idet] = 1;
            continue;
        }

        hlist = &hole_list[idet * nrow];
        plist = &part_list[idet * nrow];
        
        out = 0;
        c = 1UL << m;

        for (k=0; k < c; ++k)
        {
            rowsumprod = 1.0;
            for (i = 0; i < m; ++i)
            {
                rowsum = 1.0;
                for (j = 0; j < m; ++j)
                {
                    if (k & (1UL << j))
                    {
                        rowsum *= x[ncol * hlist[j] + plist[i]];
                    }               }
                rowsumprod += rowsum;
            }
            out += rowsumprod * (1 - ((__builtin_popcount(k) & 1) << 1));
        }
        y[idet] = out * ((m % 2 == 1) ? -1 : 1);
    }
}

void AP1roGeneralizedSenoObjective::d_overlap(const size_t ndet, const double *x, double *y)
{
    // x == parameters p_j
    // y == unwrapped overlap objective ∂σ_i/∂p_j

    std::size_t m, n, i, j, k, c;
    std::size_t *hlist, *plist;
    double rowsum, rowsumprod, out;

    for (std::size_t idet = 0; idet != ndet; ++idet){
        for (std::size_t iparam = 0; iparam != nparam; ++iparam){
            hlist = &hole_list[idet * nrow];
            plist = &part_list[idet * nrow];
            
            m = nexc_list[idet];
            if (m == 0){
                y[ndet * iparam + idet] = 0;
                continue;
            }

            std::vector<std::size_t> rows;
            std::vector<std::size_t> cols;
            for (i = 0; i < m; ++i){
                if (hlist[i] != iparam / ncol){
                    rows.push_back(hlist[i]);
                }
                if (plist[i] != iparam % ncol){
                    cols.push_back(plist[i]);
                }
            }
            m = rows.size();
            n = cols.size();
            if (m == 0 && n == 0) {
                y[ndet * iparam + idet] = 1;
                continue;
            } else if (m == nexc_list[idet] || n == nexc_list[idet] || m != n) {
                y[ndet * iparam + idet] = 0;
                continue;
            }
            out = 0;

            c = 1UL << m;
            for (k = 0; k < c; ++k){
                rowsumprod = 1.0;
                for (i = 0; i < m; ++i){
                    rowsum = 1.0;
                    for (j = 0; j < m; ++j){
                        if (k & (1UL << j)){
                            rowsum *= x[ncol * hlist[j] + plist[i]];
                        }
                    }
                    rowsumprod *= rowsum;
                }
                out += rowsumprod * (1 - ((__builtin_popcountll(k) & 1) << 1));
            }
            y[ndet * iparam + idet] = out * ((m % 2 == 1) ? -1 : 1);

        }
    }
}

} // namespace pyci
