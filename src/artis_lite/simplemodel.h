#ifndef SIMPLEMODEL_H
#define SIMPLEMODEL_H

#include <vector>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <ModelParameters.hpp>

#include <artis_lite/simpletrace.h>

using namespace std;

//useless - for signature compatibility
namespace artis { namespace kernel {
enum ValueTypeID { DOUBLE, INT, BOOL, DOUBLE_VECTOR, INT_VECTOR, BOOL_VECTOR, STRING, USER };
}}

class AbstractSimpleModel {
public:
    vector< vector< AbstractSimpleModel * >> subModels;
    virtual void compute(double t, bool update = false) = 0;
    virtual void init(double t, const ecomeristem::ModelParameters& parameters) = 0;
    virtual const double getVal(unsigned int i) = 0;
};

class SimpleView {
private:
    typedef std::map < std::string, vector < unsigned int > > Selectors;
    Selectors _selectors;
    AbstractSimpleModel * _model;
    double _begin = -1;
public:
    typedef std::vector < std::pair < double, double > > Value;
    typedef map < string, Value > Values; //var, <day,val>
    Values _values;

    void selector(const string& name, artis::kernel::ValueTypeID /*type*/, const vector < unsigned int >& chain)  {
        _selectors[name] = chain;
        _values[name] = vector < pair < double, double > >();
    }

    Values values() {return _values;}
    void attachModel(AbstractSimpleModel * m) { _model = m; }
    double get(double t, string name) { return _values[name][t-_begin].second; }
    double begin() {return _begin;}
    double end() {return _values.begin()->second.back().first;}

    void observe(double time) {
        if(_begin < 0)
            _begin = time;
        for (typename Selectors::const_iterator it = _selectors.begin(); it != _selectors.end(); ++it) {
            AbstractSimpleModel * model = _model;
            if (it->second.size() > 1) {
                size_t i = 0;
                while (i < it->second.size() - 1 and model) {
                    model = model->subModels[it->second[i]][0];
                    ++i;
                }
            }

            if (model) {
                double val = model->getVal(it->second.back());
                _values[it->first].push_back(std::make_pair(time, val));
            }
        }
    }
};

class SimpleObserver {
private:
    SimpleView * v;
    AbstractSimpleModel * _model;
public:
    typedef std::map < std::string, SimpleView* > Views;
    SimpleObserver(AbstractSimpleModel * model): _model(model){}
    ~SimpleObserver(){}
    std::map < std::string, SimpleView * > views() const {
        return std::map < std::string, SimpleView * > {{"view",v}};
    }
    void observe(double t){ v->observe(t); }
    void attachView(const std::string& /*name*/, SimpleView * view) {
        v = view;
        v->attachModel(_model);
    }
};

class SimpleContext {
private:
    double _start, _end;
public:
    SimpleContext(double start, double end): _start(start), _end(end) {}
    double begin() {return _start;}
    double end() {return _end;}
};

template < typename T, typename U, typename V >
class SimpleSimulator {
private:
    T * _model;
    SimpleObserver _observer;
public:
    SimpleSimulator(T * model, U parameters) : _model(model), _observer(model) {}
    ~SimpleSimulator() {delete _model;}
    void init(double time, const V& parameters) { _model->init(time, parameters); }
    void attachView(const string& name, SimpleView * view){ _observer.attachView(name, view);}
    const SimpleObserver& observer() const { return _observer; }
    void run(SimpleContext & context) {
        for (double t = context.begin(); t <= context.end(); t++) {
            (*_model)(t);
            _observer.observe(t);
        }
    }
};

template < typename T >
class SimpleModel : public AbstractSimpleModel {
private:
    template<typename U>
    struct id { typedef U type; };

protected:
    double last_time;
    vector<double T::*> i_double;
    vector<int T::*> i_int;
    vector<bool T::*> i_bool;
    vector<flag T::*> i_flag;
    vector<double T::*> e_double;
    vector<int T::*> e_int;
    vector<bool T::*> e_bool;
    vector<flag T::*> e_flag;

public:
    SimpleModel() {
        i_double = vector<double T::*>(80);
        i_int = vector<int T::*>(80);
        i_bool = vector<bool T::*>(80);
        i_flag = vector<flag T::*>(80);
        e_double = vector<double T::*>(80);
        e_int = vector<int T::*>(80);
        e_bool = vector<bool T::*>(80);
        e_flag = vector<flag T::*>(80);
    }

    const double getVal (unsigned int i) {
        if(i_double.size() > i && i_double[i] != nullptr) return get<double>(0,i);
        else if(i_int.size() > i && i_int[i] != nullptr) return static_cast<double>(get<int>(0,i));
        else if(i_bool.size() > i && i_bool[i] != nullptr) return static_cast<double>(get<bool>(0,i));
        else if(i_flag.size() > i && i_flag[i] != nullptr) return static_cast<double>(get<flag>(0,i));
    }

    virtual void operator()(double t) {this->compute(t);}
    void internal_(unsigned int index, const string& /*n*/, double T::* var) {i_double[index] = var;}
    void internal_(unsigned int index, const string& /*n*/, int T::* var) {i_int[index] = var;}
    void internal_(unsigned int index, const string& /*n*/, bool T::* var) {i_bool[index] = var;}
    void internal_(unsigned int index, const string& /*n*/, flag T::* var) {i_flag[index] = var;}
    void external_(unsigned int index, const string& /*n*/, double T::* var) {e_double[index] = var;}
    void external_(unsigned int index, const string& /*n*/, int T::* var) {e_int[index] = var;}
    void external_(unsigned int index, const string& /*n*/, bool T::* var) {e_bool[index] = var;}
    void external_(unsigned int index, const string& /*n*/, flag T::* var) {e_flag[index] = var;}
    template < typename W > W get(double t, unsigned int index) { return _get(t, index, id<W>()); }
    template < typename W, typename U > W get(double t, unsigned int index) {return get<W>(t,index);}
    template < typename W > void put(double t, unsigned int index, W value) {_put(t, index, value);}
    void subModel(unsigned int index, AbstractSimpleModel * model) { setsubmodel(index, model); }

    void setsubmodel(unsigned int index, AbstractSimpleModel * model) {
        if(subModels.size() <= index)
            while(subModels.size() <= index)
                subModels.push_back(vector<AbstractSimpleModel*>(0));
        subModels[index].push_back(model);
    }

private:
    double _get(double /*t*/, unsigned int index, id<double>){return static_cast<SimpleModel<T>*>(this)->*static_cast<double SimpleModel<T>::*>(i_double[index]);}
    int _get(double /*t*/, unsigned int index, id<int>){return static_cast<SimpleModel<T>*>(this)->*static_cast<int SimpleModel<T>::*>(i_int[index]);}
    bool _get(double /*t*/, unsigned int index, id<bool>){return static_cast<SimpleModel<T>*>(this)->*static_cast<bool SimpleModel<T>::*>(i_bool[index]);}
    flag _get(double /*t*/, unsigned int index, id<flag>){return static_cast<SimpleModel<T>*>(this)->*static_cast<flag SimpleModel<T>::*>(i_flag[index]);}
    void _put(double /*t*/, unsigned int index, double value) {static_cast<SimpleModel<T>*>(this)->*static_cast<double SimpleModel<T>::*>(e_double[index]) = value;}
    void _put(double /*t*/, unsigned int index, int value) {static_cast<SimpleModel<T>*>(this)->*static_cast<int SimpleModel<T>::*>(e_int[index]) = value;}
    void _put(double /*t*/, unsigned int index, bool value) {static_cast<SimpleModel<T>*>(this)->*static_cast<bool SimpleModel<T>::*>(e_bool[index]) = value;}
    void _put(double /*t*/, unsigned int index, flag value) {static_cast<SimpleModel<T>*>(this)->*static_cast<flag SimpleModel<T>::*>(e_flag[index]) = value;}
};

#define DOUBLEESCAPE(a) #a
#define ESCAPEQUOTE(a) DOUBLEESCAPE(a)
#define Internal(index, var) internal_(index, string(ESCAPEQUOTE(index)), var)
#define External(index, var) external_(index, string(ESCAPEQUOTE(index)), var)

#endif // SIMPLEMODEL_H
