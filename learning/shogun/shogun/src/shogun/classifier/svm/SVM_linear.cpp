/*
 * Copyright (c) 2007-2009 The LIBLINEAR Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither name of copyright holders nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <shogun/lib/config.h>
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_LAPACK
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <shogun/mathematics/Math.h>
#include <shogun/classifier/svm/SVM_linear.h>
#include <shogun/classifier/svm/Tron.h>

using namespace shogun;

l2r_lr_fun::l2r_lr_fun(const problem *p, float64_t Cp, float64_t Cn)
{
	int i;
	int l=p->l;
	int *y=p->y;

	this->prob = p;

	z = SG_MALLOC(double, l);
	D = SG_MALLOC(double, l);
	C = SG_MALLOC(double, l);

	for (i=0; i<l; i++)
	{
		if (y[i] == 1)
			C[i] = Cp;
		else
			C[i] = Cn;
	}
}

l2r_lr_fun::~l2r_lr_fun()
{
	SG_FREE(z);
	SG_FREE(D);
	SG_FREE(C);
}


double l2r_lr_fun::fun(double *w)
{
	int i;
	double f=0;
	int *y=prob->y;
	int l=prob->l;
	int32_t n=prob->n;

	Xv(w, z);
	for(i=0;i<l;i++)
	{
		double yz = y[i]*z[i];
		if (yz >= 0)
			f += C[i]*log(1 + exp(-yz));
		else
			f += C[i]*(-yz+log(1 + exp(yz)));
	}
	f += 0.5 *CMath::dot(w,w,n);

	return(f);
}

void l2r_lr_fun::grad(double *w, double *g)
{
	int i;
	int *y=prob->y;
	int l=prob->l;
	int w_size=get_nr_variable();

	for(i=0;i<l;i++)
	{
		z[i] = 1/(1 + exp(-y[i]*z[i]));
		D[i] = z[i]*(1-z[i]);
		z[i] = C[i]*(z[i]-1)*y[i];
	}
	XTv(z, g);

	for(i=0;i<w_size;i++)
		g[i] = w[i] + g[i];
}

int l2r_lr_fun::get_nr_variable(void)
{
	return prob->n;
}

void l2r_lr_fun::Hv(double *s, double *Hs)
{
	int i;
	int l=prob->l;
	int w_size=get_nr_variable();
	double *wa = SG_MALLOC(double, l);

	Xv(s, wa);
	for(i=0;i<l;i++)
		wa[i] = C[i]*D[i]*wa[i];

	XTv(wa, Hs);
	for(i=0;i<w_size;i++)
		Hs[i] = s[i] + Hs[i];
	SG_FREE(wa);
}

void l2r_lr_fun::Xv(double *v, double *res_Xv)
{
	int32_t l=prob->l;
	int32_t n=prob->n;
	float64_t bias=0;

	if (prob->use_bias)
	{
		n--;
		bias=v[n];
	}

	prob->x->dense_dot_range(res_Xv, 0, l, NULL, v, n, bias);
}

void l2r_lr_fun::XTv(double *v, double *res_XTv)
{
	int l=prob->l;
	int32_t n=prob->n;

	memset(res_XTv, 0, sizeof(double)*prob->n);

	if (prob->use_bias)
		n--;

	for (int32_t i=0;i<l;i++)
	{
		prob->x->add_to_dense_vec(v[i], i, res_XTv, n);

		if (prob->use_bias)
			res_XTv[n]+=v[i];
	}
}

l2r_l2_svc_fun::l2r_l2_svc_fun(const problem *p, double Cp, double Cn)
{
	int i;
	int l=p->l;
	int *y=p->y;

	this->prob = p;

	z = SG_MALLOC(double, l);
	D = SG_MALLOC(double, l);
	C = SG_MALLOC(double, l);
	I = SG_MALLOC(int, l);

	for (i=0; i<l; i++)
	{
		if (y[i] == 1)
			C[i] = Cp;
		else
			C[i] = Cn;
	}
}

l2r_l2_svc_fun::~l2r_l2_svc_fun()
{
	SG_FREE(z);
	SG_FREE(D);
	SG_FREE(C);
	SG_FREE(I);
}

double l2r_l2_svc_fun::fun(double *w)
{
	int i;
	double f=0;
	int *y=prob->y;
	int l=prob->l;
	int w_size=get_nr_variable();

	Xv(w, z);
	for(i=0;i<l;i++)
	{
		z[i] = y[i]*z[i];
		double d = 1-z[i];
		if (d > 0)
			f += C[i]*d*d;
	}
	f += 0.5*CMath::dot(w, w, w_size);

	return(f);
}

void l2r_l2_svc_fun::grad(double *w, double *g)
{
	int i;
	int *y=prob->y;
	int l=prob->l;
	int w_size=get_nr_variable();

	sizeI = 0;
	for (i=0;i<l;i++)
		if (z[i] < 1)
		{
			z[sizeI] = C[i]*y[i]*(z[i]-1);
			I[sizeI] = i;
			sizeI++;
		}
	subXTv(z, g);

	for(i=0;i<w_size;i++)
		g[i] = w[i] + 2*g[i];
}

int l2r_l2_svc_fun::get_nr_variable(void)
{
	return prob->n;
}

void l2r_l2_svc_fun::Hv(double *s, double *Hs)
{
	int i;
	int l=prob->l;
	int w_size=get_nr_variable();
	double *wa = SG_MALLOC(double, l);

	subXv(s, wa);
	for(i=0;i<sizeI;i++)
		wa[i] = C[I[i]]*wa[i];

	subXTv(wa, Hs);
	for(i=0;i<w_size;i++)
		Hs[i] = s[i] + 2*Hs[i];
	SG_FREE(wa);
}

void l2r_l2_svc_fun::Xv(double *v, double *res_Xv)
{
	int32_t l=prob->l;
	int32_t n=prob->n;
	float64_t bias=0;

	if (prob->use_bias)
	{
		n--;
		bias=v[n];
	}

	prob->x->dense_dot_range(res_Xv, 0, l, NULL, v, n, bias);
}

void l2r_l2_svc_fun::subXv(double *v, double *res_Xv)
{
	int32_t n=prob->n;
	float64_t bias=0;

	if (prob->use_bias)
	{
		n--;
		bias=v[n];
	}

	prob->x->dense_dot_range_subset(I, sizeI, res_Xv, NULL, v, n, bias);

	/*for (int32_t i=0;i<sizeI;i++)
	{
		res_Xv[i]=prob->x->dense_dot(I[i], v, n);

		if (prob->use_bias)
			res_Xv[i]+=bias;
	}*/
}

void l2r_l2_svc_fun::subXTv(double *v, double *XTv)
{
	int32_t n=prob->n;

	if (prob->use_bias)
		n--;

	memset(XTv, 0, sizeof(float64_t)*prob->n);
	for (int32_t i=0;i<sizeI;i++)
	{
		prob->x->add_to_dense_vec(v[i], I[i], XTv, n);

		if (prob->use_bias)
			XTv[n]+=v[i];
	}
}

// A coordinate descent algorithm for 
// multi-class support vector machines by Crammer and Singer
//
//  min_{\alpha}  0.5 \sum_m ||w_m(\alpha)||^2 + \sum_i \sum_m e^m_i alpha^m_i
//    s.t.     \alpha^m_i <= C^m_i \forall m,i , \sum_m \alpha^m_i=0 \forall i
// 
//  where e^m_i = 0 if y_i  = m,
//        e^m_i = 1 if y_i != m,
//  C^m_i = C if m  = y_i, 
//  C^m_i = 0 if m != y_i, 
//  and w_m(\alpha) = \sum_i \alpha^m_i x_i 
//
// Given: 
// x, y, C
// eps is the stopping tolerance
//
// solution will be put in w

#define GETI(i) (prob->y[i])
// To support weights for instances, use GETI(i) (i)

Solver_MCSVM_CS::Solver_MCSVM_CS(const problem *p, int n_class, double *weighted_C, double epsilon, int max_it)
{
	this->w_size = prob->n;
	this->l = prob->l;
	this->nr_class = n_class;
	this->eps = epsilon;
	this->max_iter = max_it;
	this->prob = p;
	this->C = weighted_C;
	this->B = SG_MALLOC(double, nr_class);
	this->G = SG_MALLOC(double, nr_class);
}

Solver_MCSVM_CS::~Solver_MCSVM_CS()
{
	SG_FREE(B);
	SG_FREE(G);
}

int compare_double(const void *a, const void *b)
{
	if(*(double *)a > *(double *)b)
		return -1;
	if(*(double *)a < *(double *)b)
		return 1;
	return 0;
}

void Solver_MCSVM_CS::solve_sub_problem(double A_i, int yi, double C_yi, int active_i, double *alpha_new)
{
	int r;
	double *D=CMath::clone_vector(B, active_i);

	if(yi < active_i)
		D[yi] += A_i*C_yi;
	qsort(D, active_i, sizeof(double), compare_double);

	double beta = D[0] - A_i*C_yi;
	for(r=1;r<active_i && beta<r*D[r];r++)
		beta += D[r];

	beta /= r;
	for(r=0;r<active_i;r++)
	{
		if(r == yi)
			alpha_new[r] = CMath::min(C_yi, (beta-B[r])/A_i);
		else
			alpha_new[r] = CMath::min((double)0, (beta - B[r])/A_i);
	}
	SG_FREE(D);
}

bool Solver_MCSVM_CS::be_shrunk(int i, int m, int yi, double alpha_i, double minG)
{
	double bound = 0;
	if(m == yi)
		bound = C[GETI(i)];
	if(alpha_i == bound && G[m] < minG)
		return true;
	return false;
}

void Solver_MCSVM_CS::Solve(double *w)
{
	int i, m, s;
	int iter = 0;
	double *alpha =  SG_MALLOC(double, l*nr_class);
	double *alpha_new = SG_MALLOC(double, nr_class);
	int *index = SG_MALLOC(int, l);
	double *QD = SG_MALLOC(double, l);
	int *d_ind = SG_MALLOC(int, nr_class);
	double *d_val = SG_MALLOC(double, nr_class);
	int *alpha_index = SG_MALLOC(int, nr_class*l);
	int *y_index = SG_MALLOC(int, l);
	int active_size = l;
	int *active_size_i = SG_MALLOC(int, l);
	double eps_shrink = CMath::max(10.0*eps, 1.0); // stopping tolerance for shrinking
	bool start_from_all = true;
	// initial
	for(i=0;i<l*nr_class;i++)
		alpha[i] = 0;
	for(i=0;i<w_size*nr_class;i++)
		w[i] = 0; 
	for(i=0;i<l;i++)
	{
		for(m=0;m<nr_class;m++)
			alpha_index[i*nr_class+m] = m;

		QD[i] = prob->x->dot(i, prob->x,i);

		active_size_i[i] = nr_class;
		y_index[i] = prob->y[i];
		index[i] = i;
	}

	while(iter < max_iter) 
	{
		double stopping = -CMath::INFTY;
		for(i=0;i<active_size;i++)
		{
			int j = i+rand()%(active_size-i);
			CMath::swap(index[i], index[j]);
		}
		for(s=0;s<active_size;s++)
		{
			i = index[s];
			double Ai = QD[i];
			double *alpha_i = &alpha[i*nr_class];
			int *alpha_index_i = &alpha_index[i*nr_class];

			if(Ai > 0)
			{
				for(m=0;m<active_size_i[i];m++)
					G[m] = 1;
				if(y_index[i] < active_size_i[i])
					G[y_index[i]] = 0;

				SG_SNOTIMPLEMENTED;
				/* FIXME
				feature_node *xi = prob->x[i];
				while(xi->index!= -1)
				{
					double *w_i = &w[(xi->index-1)*nr_class];
					for(m=0;m<active_size_i[i];m++)
						G[m] += w_i[alpha_index_i[m]]*(xi->value);
					xi++;
				}
				*/

				double minG = CMath::INFTY;
				double maxG = -CMath::INFTY;
				for(m=0;m<active_size_i[i];m++)
				{
					if(alpha_i[alpha_index_i[m]] < 0 && G[m] < minG)
						minG = G[m];
					if(G[m] > maxG)
						maxG = G[m];
				}
				if(y_index[i] < active_size_i[i])
					if(alpha_i[prob->y[i]] < C[GETI(i)] && G[y_index[i]] < minG)
						minG = G[y_index[i]];

				for(m=0;m<active_size_i[i];m++)
				{
					if(be_shrunk(i, m, y_index[i], alpha_i[alpha_index_i[m]], minG))
					{
						active_size_i[i]--;
						while(active_size_i[i]>m)
						{
							if(!be_shrunk(i, active_size_i[i], y_index[i], 
											alpha_i[alpha_index_i[active_size_i[i]]], minG))
							{
								CMath::swap(alpha_index_i[m], alpha_index_i[active_size_i[i]]);
								CMath::swap(G[m], G[active_size_i[i]]);
								if(y_index[i] == active_size_i[i])
									y_index[i] = m;
								else if(y_index[i] == m) 
									y_index[i] = active_size_i[i];
								break;
							}
							active_size_i[i]--;
						}
					}
				}

				if(active_size_i[i] <= 1)
				{
					active_size--;
					CMath::swap(index[s], index[active_size]);
					s--;	
					continue;
				}

				if(maxG-minG <= 1e-12)
					continue;
				else
					stopping = CMath::CMath::max(maxG - minG, stopping);

				for(m=0;m<active_size_i[i];m++)
					B[m] = G[m] - Ai*alpha_i[alpha_index_i[m]] ;

				solve_sub_problem(Ai, y_index[i], C[GETI(i)], active_size_i[i], alpha_new);
				int nz_d = 0;
				for(m=0;m<active_size_i[i];m++)
				{
					double d = alpha_new[m] - alpha_i[alpha_index_i[m]];
					alpha_i[alpha_index_i[m]] = alpha_new[m];
					if(fabs(d) >= 1e-12)
					{
						d_ind[nz_d] = alpha_index_i[m];
						d_val[nz_d] = d;
						nz_d++;
					}
				}

				/* FIXME
				xi = prob->x[i];
				while(xi->index != -1)
				{
					double *w_i = &w[(xi->index-1)*nr_class];
					for(m=0;m<nz_d;m++)
						w_i[d_ind[m]] += d_val[m]*xi->value;
					xi++;
				}*/
			}
		}

		iter++;
		if(iter % 10 == 0)
		{
			SG_SINFO(".");
		}

		if(stopping < eps_shrink)
		{
			if(stopping < eps && start_from_all == true)
				break;
			else
			{
				active_size = l;
				for(i=0;i<l;i++)
					active_size_i[i] = nr_class;
				SG_SINFO("*");
				eps_shrink = CMath::max(eps_shrink/2, eps);
				start_from_all = true;
			}
		}
		else
			start_from_all = false;
	}

	SG_SINFO("\noptimization finished, #iter = %d\n",iter);
	if (iter >= max_iter)
		SG_SINFO("Warning: reaching max number of iterations\n");

	// calculate objective value
	double v = 0;
	int nSV = 0;
	for(i=0;i<w_size*nr_class;i++)
		v += w[i]*w[i];
	v = 0.5*v;
	for(i=0;i<l*nr_class;i++)
	{
		v += alpha[i];
		if(fabs(alpha[i]) > 0)
			nSV++;
	}
	for(i=0;i<l;i++)
		v -= alpha[i*nr_class+prob->y[i]];
	SG_SINFO("Objective value = %lf\n",v);
	SG_SINFO("nSV = %d\n",nSV);

	delete [] alpha;
	delete [] alpha_new;
	delete [] index;
	delete [] QD;
	delete [] d_ind;
	delete [] d_val;
	delete [] alpha_index;
	delete [] y_index;
	delete [] active_size_i;
}

//
// Interface functions
//

void destroy_model(struct model *model_)
{
	SG_FREE(model_->w);
	SG_FREE(model_->label);
	SG_FREE(model_);
}

void destroy_param(parameter* param)
{
	SG_FREE(param->weight_label);
	SG_FREE(param->weight);
}
#endif //HAVE_LAPACK
#endif // DOXYGEN_SHOULD_SKIP_THIS
