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
#ifdef WITH_TRACE
    map<string,vector<double>> before_compute_trace;
    map<string,vector<double>> after_compute_trace;
#endif
    vector<string> i_names;
    vector<string> e_names;
    vector< vector< AbstractSimpleModel * >> subModels;
    AbstractSimpleModel * parent;
    int childIndex;

    virtual const string & name() = 0;
    virtual const string path() = 0;
    virtual void compute(double t, bool update = false) = 0;
    virtual void init(double t, const ecomeristem::ModelParameters& parameters) = 0;
    virtual const double getDblIValue(unsigned int i) = 0;
};

class SimpleView {
private:
    typedef std::map < std::string, vector < unsigned int > > Selectors;
    Selectors _selectors;
    AbstractSimpleModel * _model;
    double begin = -1;
public:
    typedef map < string, vector < pair < double, double > > > Values; //var, <day,val>
    Values _values;

    void selector(const string& name, artis::kernel::ValueTypeID /*type*/, const vector < unsigned int >& chain)  {
        _selectors[name] = chain;
        _values[name] = vector < pair < double, double > >();
    }

    Values values() {return _values;}
    void attachModel(AbstractSimpleModel * m) { _model = m; }
    double get(double t, string name) { return _values[name][t-begin].second; }

    void observe(double time) {
        if(begin < 0)
            begin = time;
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
                double val = model->getDblIValue(it->second.back());
                _values[it->first].push_back(std::make_pair(time, val));
            }
        }
    }
};

class SimpleObserver {
public:
    SimpleView * v;
    AbstractSimpleModel * _model;

    SimpleObserver(AbstractSimpleModel * model): _model(model){

    }

    ~SimpleObserver(){}

    std::map < std::string, SimpleView * > views() const {
        std::map < std::string, SimpleView * > m;
        m.insert(std::make_pair("View", v));
        return m;
    }
    void observe(double t){v->observe(t);}
    void attachView(const std::string& /*name*/, SimpleView * view) {
        v = view;
        v->attachModel(_model);
    }
};


class SimpleContext {
    double _start, _end;
public:
    SimpleContext(double start, double end):
        _start(start), _end(end) {}
    double begin() {return _start;}
    double end() {return _end;}
};

template < typename T, typename U, typename V >
class SimpleSimulator {
public:
    T * _model;
    SimpleObserver _observer;

    SimpleSimulator(T * model, U parameters) : _model(model), _observer(model) {}
    ~SimpleSimulator() {delete _model;}

    void init(double time, const V& parameters)
    { _model->init(time, parameters); }

    void run(SimpleContext & context) {
        for (double t = context.begin(); t <= context.end(); t++) {
            (*_model)(t);
            _observer.observe(t);
        }
    }
    void attachView(const string& name, SimpleView * view){ _observer.attachView(name, view);}
    const SimpleObserver& observer() const { return _observer; }

};



template < typename T >
class SimpleModel : public AbstractSimpleModel {

protected:
    double last_time;
    vector<double T::*> i_double;
    vector<int T::*> i_int;
    vector<bool T::*> i_bool;
    vector<plant::plant_state T::*> i_plant_state;
    vector<plant::plant_phase T::*> i_plant_phase;
    vector<culm::culm_phase T::*> i_culm_phase;
    vector<internode::internode_phase T::*> i_internode_phase;
    vector<leaf::leaf_phase T::*> i_leaf_phase;
    vector<peduncle::peduncle_phase T::*> i_peduncle_phase;

    vector<double T::*> e_double;
    vector<int T::*> e_int;
    vector<bool T::*> e_bool;
    vector<plant::plant_state T::*> e_plant_state;
    vector<plant::plant_phase T::*> e_plant_phase;
    vector<culm::culm_phase T::*> e_culm_phase;
    vector<internode::internode_phase T::*> e_internode_phase;
    vector<leaf::leaf_phase T::*> e_leaf_phase;
    vector<peduncle::peduncle_phase T::*> e_peduncle_phase;

public:
    string _name;
    SimpleModel() {
        _name = typeid(T).name();
        childIndex = -1;
        parent = nullptr;

        i_double = vector<double T::*>(80);
        i_int = vector<int T::*>(80);
        i_bool = vector<bool T::*>(80);
        i_plant_state = vector<plant::plant_state T::*>(80);
        i_plant_phase = vector<plant::plant_phase T::*>(80);
        i_culm_phase = vector<culm::culm_phase T::*>(80);
        i_internode_phase = vector<internode::internode_phase T::*>(80);
        i_leaf_phase = vector<leaf::leaf_phase T::*>(80);
        i_peduncle_phase = vector<peduncle::peduncle_phase T::*>(80);

        e_double = vector<double T::*>(80);
        e_int = vector<int T::*>(80);
        e_bool = vector<bool T::*>(80);
        e_plant_state = vector<plant::plant_state T::*>(80);
        e_plant_phase = vector<plant::plant_phase T::*>(80);
        e_culm_phase = vector<culm::culm_phase T::*>(80);
        e_internode_phase = vector<internode::internode_phase T::*>(80);
        e_leaf_phase = vector<leaf::leaf_phase T::*>(80);
        e_peduncle_phase = vector<peduncle::peduncle_phase T::*>(80);
    }

    const string path() {
        return (parent == nullptr ? "" : parent->path() + "/") +
                (childIndex == -1 ? "" : "[" + to_string(childIndex) + "]") +
                _name;
    }

    const string & name() {
        return _name;
    }

    const double getDblIValue (unsigned int i) {
        if(i_double.size() > i && i_double[i] != nullptr) return get<double>(0,i);
        else if(i_int.size() > i && i_int[i] != nullptr) return static_cast<double>(get<int>(0,i));
        else if(i_bool.size() > i && i_bool[i] != nullptr) return static_cast<double>(get<bool>(0,i));
        else if(i_plant_state.size() > i && i_plant_state[i] != nullptr) return static_cast<double>(/*static_cast<int>*/(get<plant::plant_state>(0,i)));
        else if(i_plant_phase.size() > i && i_plant_phase[i] != nullptr) return static_cast<double>(/*static_cast<int>*/(get<plant::plant_phase>(0,i)));
        else if(i_culm_phase.size() > i && i_culm_phase[i] != nullptr) return static_cast<double>(/*static_cast<int>*/(get<culm::culm_phase>(0,i)));
        else if(i_internode_phase.size() > i && i_internode_phase[i] != nullptr) static_cast<double>(/*static_cast<int>*/(get<internode::internode_phase>(0,i)));
        else if(i_leaf_phase.size() > i && i_leaf_phase[i] != nullptr) return static_cast<double>(/*static_cast<int>*/(get<leaf::leaf_phase>(0,i)));
        else if(i_peduncle_phase.size() > i && i_peduncle_phase[i] != nullptr) return static_cast<double>(/*static_cast<int>*/(get<peduncle::peduncle_phase>(0,i)));
    }

    string getIValue(unsigned int i) {
        if(i_double.size() > i && i_double[i] != nullptr) return to_string(get<double>(0,i));
        else if(i_int.size() > i && i_int[i] != nullptr) return to_string(get<int>(0,i));
        else if(i_bool.size() > i && i_bool[i] != nullptr) return get<bool>(0,i) ? "true" : "false";
        else if(i_plant_state.size() > i && i_plant_state[i] != nullptr) return to_string(static_cast<int>(get<plant::plant_state>(0,i)));
        else if(i_plant_phase.size() > i && i_plant_phase[i] != nullptr) return to_string(static_cast<int>(get<plant::plant_phase>(0,i)));
        else if(i_culm_phase.size() > i && i_culm_phase[i] != nullptr) return to_string(static_cast<int>(get<culm::culm_phase>(0,i)));
        else if(i_internode_phase.size() > i && i_internode_phase[i] != nullptr) to_string(static_cast<int>(get<internode::internode_phase>(0,i)));
        else if(i_leaf_phase.size() > i && i_leaf_phase[i] != nullptr) return to_string(static_cast<int>(get<leaf::leaf_phase>(0,i)));
        else if(i_peduncle_phase.size() > i && i_peduncle_phase[i] != nullptr) return to_string(static_cast<int>(get<peduncle::peduncle_phase>(0,i)));
    }

    string getEValue(unsigned int i) {
        if(e_double.size() > i && e_double[i] != nullptr) return to_string(get_e<double>(0,i));
        else if(e_int.size() > i && e_int[i] != nullptr) return to_string(get_e<int>(0,i));
        else if(e_bool.size() > i && e_bool[i] != nullptr) return get_e<bool>(0,i) ? "true" : "false";
        else if(e_plant_state.size() > i && e_plant_state[i] != nullptr) return to_string(static_cast<int>(get_e<plant::plant_state>(0,i)));
        else if(e_plant_phase.size() > i && e_plant_phase[i] != nullptr) return to_string(static_cast<int>(get_e<plant::plant_phase>(0,i)));
        else if(e_culm_phase.size() > i && e_culm_phase[i] != nullptr) return to_string(static_cast<int>(get_e<culm::culm_phase>(0,i)));
        else if(e_internode_phase.size() > i && e_internode_phase[i] != nullptr) to_string(static_cast<int>(get_e<internode::internode_phase>(0,i)));
        else if(e_leaf_phase.size() > i && e_leaf_phase[i] != nullptr) return to_string(static_cast<int>(get_e<leaf::leaf_phase>(0,i)));
        else if(e_peduncle_phase.size() > i && e_peduncle_phase[i] != nullptr) return to_string(static_cast<int>(get_e<peduncle::peduncle_phase>(0,i)));
    }

    virtual void operator()(double t) {
#ifdef WITH_TRACE
        //        string path  = this->path();
        //        KernelInfo c;
        //        SimpleTraceElement bst(path, t, artis::utils::BEFORE_COMPUTE, c);
        //        ::SimpleTrace::trace().addElement(bst);

        //        for (int i = 0; i < i_names.size(); ++i) {
        //            if(i_names[i] == "") continue;
        //            string test = getIValue(i);
        ////            KernelInfo k(i_names[i], true, getIValue(i));
        ////            SimpleTraceElement bsst(path, t, artis::utils::BEFORE_COMPUTE, k);
        ////            ::SimpleTrace::trace().addElement(bsst);
        ////            //                if(before_compute_trace.find( i_names[i] ) == before_compute_trace.end())
        ////            //                    before_compute_trace[i_names[i]] = vector<double>();
        ////            //                before_compute_trace[i_names[i]].push_back(getIValue(i));
        //        }
        //        for (int i = 0; i < e_names.size(); ++i) {
        //            if(e_names[i] == "") continue;
        //            KernelInfo k("*" + e_names[i], true, getEValue(i));
        //            SimpleTraceElement bsst(path, t, artis::utils::BEFORE_COMPUTE, k);
        //            ::SimpleTrace::trace().addElement(bsst);
        //        }
#endif
        this->compute(t);
#ifdef WITH_TRACE
        //        SimpleTraceElement ast(path, t, artis::utils::AFTER_COMPUTE, c);
        //        ::SimpleTrace::trace().addElement(ast);
        //        for (int i = 0; i < i_names.size(); ++i) {
        //            if(i_names[i] == "") continue;
        //            KernelInfo k(i_names[i], true, getIValue(i));
        //            SimpleTraceElement asst(path, t, artis::utils::AFTER_COMPUTE, k);
        //            ::SimpleTrace::trace().addElement(asst);
        //            //                if(after_compute_trace.find( i_names[i] ) == after_compute_trace.end())
        //            //                    after_compute_trace[i_names[i]] = vector<double>();

        //            //                after_compute_trace[i_names[i]].push_back(getIValue(i));
        //        }
#endif
    }

    template <typename W>
    void add_vec(unsigned int index, W T::* var, vector<W T::*> & vec, string name = "") {
//        if(vec.size() <= index)
//            while(vec.size() <= index)
//                vec.push_back(nullptr);
        vec[index] = var;
#ifdef WITH_TRACE
        if(name != "") {
            if(i_names.size() <= index)
                while(i_names.size() <= index)
                    i_names.push_back("");
            i_names[index] = name;
        }
#endif
    }

    template <typename W>
    void add_vec2(unsigned int index, W T::* var, vector<W T::*> & vec, string name = "") {
//        if(vec.size() <= index)
//            while(vec.size() <= index)
//                vec.push_back(nullptr);
        vec[index] = var;
#ifdef WITH_TRACE
        if(name != "") {
            if(e_names.size() <= index)
                while(e_names.size() <= index)
                    e_names.push_back("");
            e_names[index] = name;
        }
#endif
    }

    void internal_(unsigned int index, const string& n, double T::* var) {add_vec<double>(index,var,i_double,n);}
    void internal_(unsigned int index, const string& n, int T::* var) {add_vec<int>(index,var,i_int,n);}
    void internal_(unsigned int index, const string& n, bool T::* var) {add_vec<bool>(index,var,i_bool,n);}
    void internal_(unsigned int index, const string& n, plant::plant_state T::* var) {add_vec<plant::plant_state>(index,var,i_plant_state,n);}
    void internal_(unsigned int index, const string& n, plant::plant_phase T::* var) {add_vec<plant::plant_phase>(index,var,i_plant_phase,n);}
    void internal_(unsigned int index, const string& n, culm::culm_phase T::* var) {add_vec<culm::culm_phase>(index,var,i_culm_phase,n);}
    void internal_(unsigned int index, const string& n, internode::internode_phase T::* var) {add_vec<internode::internode_phase>(index,var,i_internode_phase,n);}
    void internal_(unsigned int index, const string& n, leaf::leaf_phase T::* var) {add_vec<leaf::leaf_phase>(index,var,i_leaf_phase,n);}
    void internal_(unsigned int index, const string& n, peduncle::peduncle_phase T::* var) {add_vec<peduncle::peduncle_phase>(index,var,i_peduncle_phase,n);}

    void external_(unsigned int index, const string& n, double T::* var) {add_vec2<double>(index,var,e_double,n);}
    void external_(unsigned int index, const string& n, int T::* var) {add_vec2<int>(index,var,e_int,n);}
    void external_(unsigned int index, const string& n, bool T::* var) {add_vec2<bool>(index,var,e_bool,n);}
    void external_(unsigned int index, const string& n, plant::plant_state T::* var) {add_vec2<plant::plant_state>(index,var,e_plant_state,n);}
    void external_(unsigned int index, const string& n, plant::plant_phase T::* var) {add_vec2<plant::plant_phase>(index,var,e_plant_phase,n);}
    void external_(unsigned int index, const string& n, culm::culm_phase T::* var) {add_vec2<culm::culm_phase>(index,var,e_culm_phase,n);}
    void external_(unsigned int index, const string& n, internode::internode_phase T::* var) {add_vec2<internode::internode_phase>(index,var,e_internode_phase,n);}
    void external_(unsigned int index, const string& n, leaf::leaf_phase T::* var) {add_vec2<leaf::leaf_phase>(index,var,e_leaf_phase,n);}
    void external_(unsigned int index, const string& n, peduncle::peduncle_phase T::* var) {add_vec2<peduncle::peduncle_phase>(index,var,e_peduncle_phase,n);}

    template < typename W > W get(double t, unsigned int index) { return nullptr; }
    template < typename W, typename U > W get(double t, unsigned int index) {
        return get<W>(t,index);
    }
    template <> double get<double>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<double SimpleModel<T>::*>(i_double[index]);}
    template <> int get<int>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<int SimpleModel<T>::*>(i_int[index]);}
    template <> bool get<bool>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<bool SimpleModel<T>::*>(i_bool[index]);}
    template <> plant::plant_state get<plant::plant_state>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_state SimpleModel<T>::*>(i_plant_state[index]);}
    template <> plant::plant_phase get<plant::plant_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_phase SimpleModel<T>::*>(i_plant_phase[index]);}
    template <> culm::culm_phase get<culm::culm_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<culm::culm_phase SimpleModel<T>::*>(i_culm_phase[index]);}
    template <> internode::internode_phase get<internode::internode_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<internode::internode_phase SimpleModel<T>::*>(i_internode_phase[index]);}
    template <> leaf::leaf_phase get<leaf::leaf_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<leaf::leaf_phase SimpleModel<T>::*>(i_leaf_phase[index]);}
    template <> peduncle::peduncle_phase get<peduncle::peduncle_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<peduncle::peduncle_phase SimpleModel<T>::*>(i_peduncle_phase[index]);}

    template < typename W > W get_e(double t, unsigned int index) {
        return nullptr;
    }
    template < typename W, typename U > W get_e(double t, unsigned int index) {
        return get_e<W>(t,index);
    }
    template <> double get_e<double>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<double SimpleModel<T>::*>(e_double[index]);
    }
    template <> int get_e<int>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<int SimpleModel<T>::*>(e_int[index]);
    }
    template <> bool get_e<bool>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<bool SimpleModel<T>::*>(e_bool[index]);
    }
    template <> plant::plant_state get_e<plant::plant_state>(double t, unsigned int index){
        return static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_state SimpleModel<T>::*>(e_plant_state[index]);
    }
    template <> plant::plant_phase get_e<plant::plant_phase>(double t, unsigned int index){
        return static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_phase SimpleModel<T>::*>(e_plant_phase[index]);
    }
    template <> culm::culm_phase get_e<culm::culm_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<culm::culm_phase SimpleModel<T>::*>(e_culm_phase[index]);
    }
    template <> internode::internode_phase get_e<internode::internode_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<internode::internode_phase SimpleModel<T>::*>(e_internode_phase[index]);
    }
    template <> leaf::leaf_phase get_e<leaf::leaf_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<leaf::leaf_phase SimpleModel<T>::*>(e_leaf_phase[index]);
    }
    template <> peduncle::peduncle_phase get_e<peduncle::peduncle_phase>(double t, unsigned int index) {
        return static_cast<SimpleModel<T>*>(this)->*static_cast<peduncle::peduncle_phase SimpleModel<T>::*>(e_peduncle_phase[index]);
    }


    void tracePut(int idx, string value, double t) {
#ifdef WITH_TRACE
        KernelInfo k(e_names[idx], false, value);
        SimpleTraceElement asst(path(), t, artis::utils::PUT, k);
        ::SimpleTrace::trace().addElement(asst);
#endif
    }

    template < typename W > void put(double t, unsigned int index, W value) {
        cout<<"nospecialization"<<"\n";
    }
    template <> void put<double>(double t, unsigned int index, double value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<double SimpleModel<T>::*>(e_double[index]) = value;
    }
    template <> void put<int>(double t, unsigned int index, int value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<int SimpleModel<T>::*>(e_int[index]) = value;
    }
    template <> void put<bool>(double t, unsigned int index, bool value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<bool SimpleModel<T>::*>(e_bool[index]) = value;
    }
    template <> void put<plant::plant_state>(double t, unsigned int index, plant::plant_state value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_state SimpleModel<T>::*>(e_plant_state[index]) = value;
    }
    template <> void put<plant::plant_phase>(double t, unsigned int index, plant::plant_phase value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<plant::plant_phase SimpleModel<T>::*>(e_plant_phase[index]) = value;
    }
    template <> void put<culm::culm_phase>(double t, unsigned int index, culm::culm_phase value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<culm::culm_phase SimpleModel<T>::*>(e_culm_phase[index]) = value;
    }
    template <> void put<internode::internode_phase>(double t, unsigned int index, internode::internode_phase value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<internode::internode_phase SimpleModel<T>::*>(e_internode_phase[index]) = value;
    }
    template <> void put<leaf::leaf_phase>(double t, unsigned int index, leaf::leaf_phase value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<leaf::leaf_phase SimpleModel<T>::*>(e_leaf_phase[index]) = value;
    }
    template <> void put<peduncle::peduncle_phase>(double t, unsigned int index, peduncle::peduncle_phase value) {
        static_cast<SimpleModel<T>*>(this)->*static_cast<peduncle::peduncle_phase SimpleModel<T>::*>(e_peduncle_phase[index]) = value;
    }

    void subModel(unsigned int index, AbstractSimpleModel * model) {
        setsubmodel(index, model);
    }

    void setsubmodel(unsigned int index, AbstractSimpleModel * model) {
        //** MAP STYLE **//
        //        if(subModels.find( model->name() ) == subModels.end())
        //            subModels[model->name()] = vector<AbstractSimpleModel*>();
        //        subModels[model->name()].push_back(model);
        //        model->parent = this;
        //        model->childIndex = subModels[model->name()].size() - 1;
        /***************************/

        //** VECTOR STYLE **//
        if(subModels.size() <= index)
            while(subModels.size() <= index)
                subModels.push_back(vector<AbstractSimpleModel*>());
        subModels[index].push_back(model);
        model->parent = this;
        model->childIndex = subModels[index].size() - 1;
    }
};

#define DOUBLEESCAPE(a) #a
#define ESCAPEQUOTE(a) DOUBLEESCAPE(a)
#define Internal(index, var) internal_(index, string(ESCAPEQUOTE(index)), var)
#define External(index, var) external_(index, string(ESCAPEQUOTE(index)), var)


#endif // SIMPLEMODEL_H