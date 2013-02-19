/** 
  * @file
  * @copyright University of Warsaw
  * @section LICENSE
  * GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
  */

#pragma once
#include "detail/solver_pressure_common.hpp"
#include "../formulae/nabla_formulae.hpp" //gradient, diveregnce

namespace advoocat
{
  namespace solvers
  {
    template <class inhomo_solver_t, int u, int w, int tht>
    class pressure_mr : public detail::pressure_solver_common<inhomo_solver_t, u, w, tht>
    {
      public:

      using parent_t = detail::pressure_solver_common<inhomo_solver_t, u, w, tht>;
      typedef typename parent_t::real_t real_t;

      typename parent_t::arr_t Phi; 
      //TODO probably don't need those
      typename parent_t::arr_t tmp_u, tmp_w, tmp_x, tmp_z;
      typename parent_t::arr_t err, lap_err, tmp_e1, tmp_e2;

      private:

      void ini_pressure()
      {
	// dt/2 * (Prs-Prs_amb) / rho
	Phi(this->i^this->halo, this->j^this->halo) = real_t(0);
      }

      void pressure_solver_update(real_t dt)
      {
	using namespace arakawa_c;
	using formulae::nabla_op::grad;
	using formulae::nabla_op::div;

	real_t rho = 1.;   //TODO    

	int halo = this->halo;
	rng_t &i = this->i;
	rng_t &j = this->j;

	tmp_u = this->psi(u);
	tmp_w = this->psi(w);

        this->mem->barrier();
        this->xchng(Phi,   i^halo, j^halo);
        this->xchng(tmp_u, i^halo, j^halo);
        this->xchng(tmp_w, i^halo, j^halo);

        tmp_x(i, j) = rho * (tmp_u(i, j) - grad<0>(Phi(i^halo, j^halo), i, j, real_t(1)));
        tmp_z(i, j) = rho * (tmp_w(i, j) - grad<1>(Phi(i^halo, j^halo), j, i, real_t(1)));

        this->mem->barrier();
        this->xchng(tmp_x, i^halo, j^halo);
        this->xchng(tmp_z, i^halo, j^halo);

        err(i, j) = - 1./ rho * div(tmp_x(i^halo,j^halo), tmp_z(i^halo, j^halo), i, j, real_t(1), real_t(1)); //error

std::cerr<<"--------------------------------------------------------------"<<std::endl;
	//pseudo-time loop
	real_t error = 1.;
	while (error > .00001)
	{
          this->mem->barrier();
          this->xchng(err, i^halo, j^halo);

          tmp_e1(i, j) = grad<0>(err(i^halo, j^halo), i, j, real_t(1));
          tmp_e2(i, j) = grad<1>(err(i^halo, j^halo), j, i, real_t(1));
          this->mem->barrier();
          this->xchng(tmp_e1, i^halo, j^halo);
          this->xchng(tmp_e2, i^halo, j^halo);
          this->mem->barrier();
 
          lap_err(i,j) = div(tmp_e1(i^halo,j^halo), tmp_e2(i^halo, j^halo), i, j, real_t(1), real_t(1)); //laplasjan(error)

          tmp_e1(i,j) = err(i,j)*lap_err(i,j);
          tmp_e2(i,j) = lap_err(i,j)*lap_err(i,j);
          real_t beta = - this->mem->sum(tmp_e1(i,j))/this->mem->sum(tmp_e2(i,j));

          Phi(i, j) += beta * err(i, j);
          err(i, j) += beta * lap_err(i, j);

          error = std::max(std::abs(this->mem->max(err(i,j))), std::abs(this->mem->min(err(i,j))));
          this->iters++;
std::cerr<<error<<std::endl;
	}
	//end of pseudo_time loop
        this->mem->barrier();
	this->xchng(this->Phi, i^halo, j^halo);

	tmp_u(i, j) = - grad<0>(Phi(i^halo, j^halo), i, j, real_t(1));
	tmp_w(i, j) = - grad<1>(Phi(i^halo, j^halo), j, i, real_t(1));
      }

      void pressure_solver_apply(real_t dt)
      {
	auto U = this->psi(u);
	auto W = this->psi(w);

	U += tmp_u;
	W += tmp_w;
      }

      public:

      struct params_t : parent_t::params_t { };

      // ctor
      pressure_mr(
	typename parent_t::mem_t *mem,
        typename parent_t::bc_p &bcxl,
        typename parent_t::bc_p &bcxr,
        typename parent_t::bc_p &bcyl,
        typename parent_t::bc_p &bcyr,
	const rng_t &i,
	const rng_t &j,
	const params_t &p
      ) :
	parent_t(mem, bcxl, bcxr, bcyl, bcyr, i, j, p),
        // (i, j)
        lap_err(mem->tmp[std::string(__FILE__)][0][0]),
        // (i^hlo, j^hlo))
	err(mem->tmp[std::string(__FILE__)][0][1]),
	tmp_x(mem->tmp[std::string(__FILE__)][0][2]),
	tmp_z(mem->tmp[std::string(__FILE__)][0][3]),
	tmp_u(mem->tmp[std::string(__FILE__)][0][4]),
	tmp_w(mem->tmp[std::string(__FILE__)][0][5]),
	Phi(mem->tmp[std::string(__FILE__)][0][6]),
	tmp_e1(mem->tmp[std::string(__FILE__)][0][7]),
	tmp_e2(mem->tmp[std::string(__FILE__)][0][8])
      {}

      static void alloc(typename parent_t::mem_t *mem, const int nx, const int ny)
      {
        parent_t::alloc(mem, nx, ny);

        const std::string file(__FILE__);
        const rng_t i(0, nx-1), j(0, ny-1);
        const int halo = parent_t::halo;

        // temporary fields
        mem->tmp[file].push_back(new arrvec_t<typename parent_t::arr_t>());
        {
          for (int n=0; n < 1; ++n) 
            mem->tmp[file].back().push_back(new typename parent_t::arr_t(i, j)); 
          for (int n=0; n < 8; ++n) 
            mem->tmp[file].back().push_back(new typename parent_t::arr_t( i^halo, j^halo )); 
        }
      }
    }; 
  }; // namespace solvers
}; // namespace advoocat
