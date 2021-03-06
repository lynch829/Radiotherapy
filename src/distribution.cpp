#include "distribution.h"

UniformDist::UniformDist(IloNum l, IloNum u)
{
	if(!(u>l)){
		cout << "Error: upperbound is lower than lower bound " << l << " " << u << endl;
	}

	b = u;
	a = l;
}

void UniformDist::operator= (UniformDist & arg)
{
   a = arg.a;
   b = arg.b;
}

IloNum UniformDist::cdf(IloNum t)
{
	if(t <= a){
		return 0.;
	}else if(t < b){
		return (t-a)/(b-a);
	}

	return 1.;
}

IloNum UniformDist::intTo(IloNum T)
{
	if( T <= a){
		return 0;
	}else if(T < b){
		return .5*((T-a)/(b-a))*(T-a);
	}

	return .5*(b-a)+(T-b);
}

IloExpr UniformDist::intTo(IloExpr & T, IloNum ti)
{
	if(ti <= a){
		return T*0.; // can't just return 0 unless i pass IloEnv
	}else if(ti <= b){
		return (((ti-a)*(ti-a))/(2*(b-a)))+((ti-a)/(b-a))*(T-ti);
	}

	return .5*(b-a)+T-b;
}

IloNum UniformDist::intToInf(IloNum T)
{
	if(T >= b){
		return 0.;
	}else if(T > a){
		return .5*(1-((T-a)/(b-a)))*(b-T);
	}

	return .5*(b-a)+(a-T);
}

IloExpr UniformDist::intToInf(IloExpr & T, IloNum ti)
{
	if(ti <= a){
		return .5*(b-a)+a-T;
	}else if(ti <= b){
		return (((b-ti)*(b-ti))/(2*(b-a)))+((ti-b)/(b-a))*(T-ti);
	}

	return 0.*T;
}

vector<IloNum> UniformDist::getTRange(unsigned int n)
{
	vector<IloNum> range(n);

	IloNum spacing = (b-a)/(IloNum(n)-1.);

	for(unsigned int i=0;i<range.size();++i)
	{
		range[i] = a+spacing*IloNum(i);
	}

	return range;
}

vector<IloNum> UniformDist::getLargerTRange(unsigned int n)
{
	vector<IloNum> range(n);

	IloNum low, up;
	low = a - (b-a);
	up = b + (b-a);

	IloNum spacing = (up-low)/(IloNum(n)-1.);

	for(unsigned int i=0;i<range.size();++i)
	{
		range[i] = low + spacing*IloNum(i);
	}

	return range;
}

void UniformDist::print(string fname, IloNum lower, IloNum upper, IloInt count)
{
	std::ofstream out;

	out.open(fname.c_str());

	if(!out.is_open()){
		cout << "Error: could not open " << fname << '\n';
	}

	IloNum spacing = (upper - lower)/(IloNum(count)-1);

	out << "Integral from -infinity to t\n";
	for(IloNum t = lower;lower<=upper;lower+=spacing)
	{
		out << t << '\t' << intTo(t) << '\n';
	}

	out << "Integral of survival funciton\n";

	for(IloNum t = lower;lower<=upper;lower+=spacing)
	{
		out << t << '\t' << intToInf(t) << '\n';
	}

	out.close();

}

// PiecewiseLinDist
PiecewiseLinDist::PiecewiseLinDist()
{

}

PiecewiseLinDist::~PiecewiseLinDist()
{

}

void PiecewiseLinDist::operator= (const PiecewiseLinDist & arg)
{
   data = arg.data;
}

IloNum PiecewiseLinDist::cdf(IloNum t)
{
   if( t <= data[0].t){
      return data[0].t;
   }else if(t >= data[data.size()-1].t){
      return data[data.size()-1].t;
   }

   for(unsigned i=1;i<data.size();++i)
   {
      if(data[i].t > t)
      {
         IloNum slope = (data[i].cdf_t - data[i-1].cdf_t)/
            (data[i].t - data[i-1].t);
         return slope*(t-data[i-1].t) + data[i-1].cdf_t;
      }
   }

   // should never reach here
   return 0.;
}

IloNum PiecewiseLinDist::intTo(IloNum T)
{
   if( T <= data[0].t){
      return 0.;
   }

   IloNum sum = 0.;
   for(unsigned i=1;i<data.size();++i)
   {
      if(data[i].t > T)
      {
         for(unsigned j=1;j=i;++j)
         {
            sum += (data[j-1].cdf_t+data[j].cdf_t)*(data[j].t-data[j-1].t)/2.;
         }

         sum += (cdf(T)+data[i].cdf_t)*(T-data[i].t)/2.;
         return sum;
      }
   }

   for(unsigned i=1;i<data.size();++i)
   {
      sum += (data[i-1].cdf_t+data[i].cdf_t)*(data[i].t-data[i-1].t)/2.;
   }

   return sum;
}

IloExpr intTo(IloExpr & T, IloNum ti)
{
   return IloExpr();
}

IloNum intToInf(IloNum T)
{
   return 0.;
}

IloExpr intToInf(IloExpr & T, IloNum ti)
{
   return IloExpr();
}

// Jumpdist
JumpDist::JumpDist(IloNum t)
{
	t0 = t;
}

void JumpDist::operator=(JumpDist & arg)
{
   t0 = arg.t0;
}

IloNum JumpDist::cdf(IloNum t)
{
	if(t < t0){
		return 0.;
	}

	return 1.;
}

IloNum JumpDist::intTo(IloNum T)
{
	if(T < t0)
	{
		return 0.;
	}

	return T-t0;
}

IloExpr JumpDist::intTo( IloExpr & T)
{
	// makes the problem non-smooth
	return IloMax(T*0. , T-t0);
}

IloNum JumpDist::intToInf(IloNum T)
{
	if( T < t0)
	{
		return t0-T;
	}

	return 0.;
}

IloExpr JumpDist::intToInf( IloExpr & T)
{
	// makes the problem non-smooth
	return IloMax(T*0., t0-T);
}

vector<IloNum> JumpDist::getTRange(unsigned int n)
{
	vector<IloNum> range(n);

	IloNum spacing = (3*t0)/(IloNum(n)-1.);

	for(unsigned int i=0;i<range.size();++i)
	{
		range[i] = IloNum(i)*spacing;
	}

	return range;
}