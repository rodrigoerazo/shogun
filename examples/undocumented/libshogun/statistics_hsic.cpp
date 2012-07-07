/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2012 Heiko Strathmann
 */

#include <shogun/base/init.h>
#include <shogun/statistics/HSIC.h>
#include <shogun/kernel/GaussianKernel.h>
#include <shogun/features/DenseFeatures.h>
#include <shogun/mathematics/Statistics.h>

using namespace shogun;


void create_mean_data(SGMatrix<float64_t> target, float64_t difference)
{
	/* create data matrix for P and Q. P is a standard normal, Q is the same but
	 * has a mean difference in one dimension */
	for (index_t i=0; i<target.num_rows; ++i)
	{
		for (index_t j=0; j<target.num_cols/2; ++j)
			target(i,j)=CMath::randn_double();

		/* add mean difference in first dimension of second half of data */
		for (index_t j=target.num_cols/2; j<target.num_cols; ++j)
				target(i,j)=CMath::randn_double() + (i==0 ? difference : 0);
	}
}

void create_fixed_data_kernel(CFeatures*& features_p,
		CFeatures*& features_q, CKernel*& kernel_p, CKernel*& kernel_q)
{
	index_t m=2;
	index_t d=3;

	SGMatrix<float64_t> p(d,2*m);
	for (index_t i=0; i<2*d*m; ++i)
		p.matrix[i]=i;

	p.display_matrix("p");

	SGMatrix<float64_t> q(d,2*m);
	for (index_t i=0; i<2*d*m; ++i)
		q.matrix[i]=i+10;

	q.display_matrix("q");

	features_p=new CDenseFeatures<float64_t>(p);
	features_q=new CDenseFeatures<float64_t>(q);

	float64_t sigma_x=2;
	float64_t sigma_y=3;
	float64_t sq_sigma_x_twice=sigma_x*sigma_x*2;
	float64_t sq_sigma_y_twice=sigma_y*sigma_y*2;

	/* shoguns kernel width is different */
	kernel_p=new CGaussianKernel(10, sq_sigma_x_twice);
	kernel_q=new CGaussianKernel(10, sq_sigma_y_twice);
}

/** tests the hsic statistic for a single fixed data case and ensures
 * equality with matlab implementation */
void test_hsic_fixed()
{
	CFeatures* features_p=NULL;
	CFeatures* features_q=NULL;
	CKernel* kernel_p=NULL;
	CKernel* kernel_q=NULL;
	create_fixed_data_kernel(features_p, features_q, kernel_p, kernel_q);

	CHSIC* hsic=new CHSIC(kernel_p, kernel_q, features_p, features_q);

	/* assert matlab result */
	float64_t difference=hsic->compute_statistic();
	SG_SPRINT("hsic fixed: %f\n", difference);
	ASSERT(CMath::abs(difference-0.164761446385339)<10E-17);

	SG_UNREF(hsic);
}

void test_hsic_gamma()
{
	CFeatures* features_p=NULL;
	CFeatures* features_q=NULL;
	CKernel* kernel_p=NULL;
	CKernel* kernel_q=NULL;
	create_fixed_data_kernel(features_p, features_q, kernel_p, kernel_q);

	CHSIC* hsic=new CHSIC(kernel_p, kernel_q, features_p, features_q);
	hsic->set_null_approximation_method(HSIC_GAMMA);

	hsic->compute_p_value(1);

	SG_UNREF(hsic);
}

int main(int argc, char** argv)
{
	init_shogun_with_defaults();

//	sg_io->set_loglevel(MSG_DEBUG);

	/* all tests have been "speed up" by reducing the number of runs/samples.
	 * If you have any doubts in the results, set all num_runs to original
	 * numbers and activate asserts. If they fail, something is wrong.
	 */
	test_hsic_fixed();
	test_hsic_gamma();

	exit_shogun();
	return 0;
}

