#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include <ilcplex/ilocplex.h>
ILOSTLBEGIN
#include "sparse_matrix.h"
#include "distribution.h"
#include "set.h"
#include "opt.h"

#define NUM_T_VALS 10
#define NUM_T_FOR_DC 100
#define MAX_ITS 10000
#define EPSILON 0.000001
#define MAX(A,B) ( (A) > (B) ? (A) : (B))


// returns the expression for the dose
IloExpr D(unsigned int i, Sparse_Matrix<IloInt>& vox, IloNumVarArray& w, IloEnv & env);
IloNum D(unsigned int i, Sparse_Matrix<IloInt>& vox, IloNumArray& w_hat);

// returns \sum _{i \in A} (D(i)-Tx) 
IloExpr twelveLHS(IloEnv & env, Sparse_Matrix<IloInt>& vox, vector<unsigned int> & A,
	IloNum Tx, vector<unsigned int> & X, IloNumVarArray & w);

// returns \sum _{i \in B}(Ty-D(i)
IloExpr thirteenLHS(IloEnv & env, Sparse_Matrix<IloInt>& vox, vector<unsigned int> & B,
	IloNum Ty, vector<unsigned int> & Y, IloNumVarArray & w);

// returns the max _{t >= U} ( \max (0, D(i) - t) - card(X)*u(t-z) )
template <class Dist>
IloNum deltaX(IloNum & t, IloNum z, unsigned int cardX,
	vector<IloNum> & Dxk, Dist & u);

// returns the max _{0 <= t <= Ly} ( \max (0, t - D(i)) -card(Y)v(t+s) )
template <class Dist>
IloNum deltaY(IloNum & t, IloNum s, unsigned int cardY,
	vector<IloNum> & Dyk, Dist & v);

// A = {i \in X: D^k(i) >= T^{k_x)_X }
Set getA(vector<IloNum> & Dx, Set & X, IloNum T);
// A = {i \in Y: D^k(i) <= T^{k_y}_Y }
Set getB(vector<IloNum> & Dy, Set & Y, IloNum T);

// returns a vector of all the doses of the elements of a given set
vector<IloNum> getDoses(Sparse_Matrix<IloInt>& vox, IloNumArray & w, vector<unsigned> & organSet);

/**** Iterative Methods ****/
// From The Cutting plane algorithm from Stochastic Programming Approaches for Radiation Therapy Planning
template <class dist>
bool cuttingPlaneMethod(IloEnv & env, Opt& solution, Sparse_Matrix<IloInt> & vox,
	Set & X, Set & Y, const IloNum& Ux, const IloNum& Ly, dist & u, dist & v);

/******** Objective Functions *****************/
IloExpr minDoses(IloEnv & env, IloNumVarArray & w, Sparse_Matrix<IloInt>& vox);
IloExpr minBeamletStr(IloEnv & env, IloNumVarArray & w, Sparse_Matrix<IloInt> & vox);

template <class Dist>
IloNum deltaX(IloNum & t, IloNum z, unsigned int cardX,
	vector<IloNum> & Dxk, Dist & u)
{
	vector<IloNum> t_range = u.getTRange(NUM_T_VALS);

	IloNum max = -FLT_MAX;
	
	for(unsigned int it=0;it<t_range.size();++it)
	{
		IloNum val=0;
		for(unsigned int i=0;i<Dxk.size();++i)
		{
			val += MAX( 0.f, Dxk[i] - t_range[it]);
		}

		val -= IloNum(cardX)*u.intToInf(t_range[it]-z);

		if(val >= max){
			max = val;
			t = t_range[it];
		}
	}

	return max;
}

template <class Dist>
IloNum deltaY(IloNum & t, IloNum s, unsigned int cardY,
	vector<IloNum> & Dyk, Dist & v)
{
	vector<IloNum> t_range = v.getTRange(NUM_T_VALS);

	IloNum max = -FLT_MAX;
	
	for(unsigned int it=0;it<t_range.size();++it)
	{
		IloNum val=0;
		for(unsigned int i=0;i<Dyk.size();++i)
		{
			val += MAX( 0.f, t_range[it] - Dyk[i]);
		}

		val -= IloNum(cardY)*v.intTo(t_range[it]+s);

		if(val >= max){
			max = val;
			t = t_range[it];
		}
	}

	return max;
}

template <class dist>
bool cuttingPlaneMethod(IloEnv & env, Opt& solution, Sparse_Matrix<IloInt> & vox,
	Set & X, Set & Y, const IloNum& Ux, const IloNum& Ly, dist & u, dist & v)
{
	// booleans for solutions
	bool solved = false;

	IloInt n, m;
	m = vox.m;
	n = vox.n;

	// decision variable
	try{
		IloNumVarArray w(env, n, 0, 120);

		/******* Step 0 *********/
		unsigned int k, kx, ky;
		k = kx = ky = 1;
			
		while(!solved)
		{
			/******* Step 1 *********/
			IloModel model(env);
			// objective function
			IloExpr obj = minDoses(env, w, vox);
			// add the objective function
			model.add( IloMinimize( env, obj) );
			// begin to add constraints
			IloRangeArray constraints(env);
			// (12)
			constraints.add( twelveLHS(env, vox, X, Ux, X, w) <= IloNum(X.size())*(u.intToInf(Ux)) );
			// (13)
			constraints.add( thirteenLHS(env, vox, Y, Ly, Y, w) <= IloNum(Y.size())*(v.intTo(Ly)) );
			model.add( constraints );

			IloCplex cplex(model);
			cplex.setOut(env.getNullStream());		// silent
			cplex.setParam(IloCplex::WorkMem, 7000); // add memory
			//cplex.setParam(IloCplex::MemoryEmphasis, 1); // save memory
			//cplex.setParam(IloCplex::Threads, 1);  // set thread count

			cout << "solving" << endl;
			while(!solved)
			{
				cout << "ITERATION " << k << endl;
		
				if( !cplex.solve() ){
					env.error() << "Failed to optimize LP." << endl;
               if(!cplex.isPrimalFeasible()){
                  env.out() << "Problem is not feasible" << endl;
               }
					throw(-1);
				}

				// Get solution
				IloNum min = cplex.getObjValue();
				IloNumArray vals(env);
				cplex.getValues(vals, w);

				// calculates the doses at the current solution
				vector<IloNum> Dx, Dy;
            Dx = getDoses(vox, vals, X);
            Dy = getDoses(vox, vals, Y);

				/****** Step 2 ******/
				IloNum dx, dy, Txk, Tyk;
				dx = deltaX(Txk, 0., unsigned(X.size()), Dx, u);
				dy = deltaY(Tyk, 0., unsigned(Y.size()), Dy, v);
				cout << " dx dy " << dx << " " << dy << endl;

				// check to see if solution is optimal
				if(dx <= EPSILON && dy <= EPSILON){
							
					cout << "\n\n\n" << endl;
					cout << "****************************************" << endl;
					cout << "A solution was found!" << endl;
					cout << k << " iterations" << endl;
					cout << kx << " cuts for the OAR constraint" << endl;
					cout << ky << " cuts for the PTV constraint" << endl;

					solution.Min = cplex.getObjValue();
					cplex.getValues( solution.ArgMin, w );

					return true;
				}
		
				/****** Step 3 ******/
				if(dx > 0.){
					++kx;
					model.add( twelveLHS(env, vox, getA(Dx,X,Txk), Txk, X, w) <=
                  IloNum(X.size())*u.intToInf(Txk) );
				}

				if(dy > 0.){
					++ky;
					model.add( thirteenLHS(env, vox, getB(Dy,Y,Tyk), Tyk, Y, w) <=
                  IloNum(Y.size())*v.intTo(Tyk) );
				}

				if(k == MAX_ITS){
					cout << "Reached max iterations " << endl;
					break;
				}

				++k;
			} // adding cuts
		}
	} // try
	catch(IloException& e){
		cout << "Concert exception caught: " << e << endl;
		return false;
	}
	catch(...){
		cout << "Unknown exception caught" << endl;
		return false;
	}
	return false; // never should reach here
}


#endif // OPTIMIZE_H