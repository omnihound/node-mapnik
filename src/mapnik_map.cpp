
#include <node_buffer.h>
#include <node_version.h>

// mapnik
#include <mapnik/map.hpp>
#include <mapnik/projection.hpp>
#include <mapnik/layer.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/filter_factory.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/config_error.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/save_map.hpp>
#include <mapnik/query.hpp>
#include <mapnik/ctrans.hpp>
// icu
#include <unicode/unistr.h>



// careful, missing include gaurds: http://trac.mapnik.org/changeset/2516
//#include <mapnik/filter_featureset.hpp>

// not currently used...
//#include <mapnik/color_factory.hpp>
//#include <mapnik/hit_test_filter.hpp>
//#include <mapnik/memory_datasource.hpp>
//#include <mapnik/memory_featureset.hpp>
//#include <mapnik/params.hpp>
//#include <mapnik/feature_layer_desc.hpp>

#if defined(HAVE_CAIRO)
#include <mapnik/cairo_renderer.hpp>
#endif

// stl
#include <exception>

#include <mapnik/version.hpp>

#include "utils.hpp"
#include "mapnik_map.hpp"
#include "json_emitter.hpp"
#include "mapnik_layer.hpp"
#include "grid.h"
#include "renderer.h"



Persistent<FunctionTemplate> Map::constructor;

void Map::Initialize(Handle<Object> target) {

    HandleScope scope;

    constructor = Persistent<FunctionTemplate>::New(FunctionTemplate::New(Map::New));
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Map"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "load", load);
    NODE_SET_PROTOTYPE_METHOD(constructor, "save", save);
    NODE_SET_PROTOTYPE_METHOD(constructor, "clear", clear);
    NODE_SET_PROTOTYPE_METHOD(constructor, "from_string", from_string);
    NODE_SET_PROTOTYPE_METHOD(constructor, "toString", to_string);
    NODE_SET_PROTOTYPE_METHOD(constructor, "resize", resize);
    NODE_SET_PROTOTYPE_METHOD(constructor, "width", width);
    NODE_SET_PROTOTYPE_METHOD(constructor, "height", height);
    NODE_SET_PROTOTYPE_METHOD(constructor, "buffer_size", buffer_size);
    NODE_SET_PROTOTYPE_METHOD(constructor, "generate_hit_grid", generate_hit_grid);
    NODE_SET_PROTOTYPE_METHOD(constructor, "render_grid", render_grid);
    NODE_SET_PROTOTYPE_METHOD(constructor, "extent", extent);
    NODE_SET_PROTOTYPE_METHOD(constructor, "zoom_all", zoom_all);
    NODE_SET_PROTOTYPE_METHOD(constructor, "zoom_to_box", zoom_to_box);
    NODE_SET_PROTOTYPE_METHOD(constructor, "render", render);
    NODE_SET_PROTOTYPE_METHOD(constructor, "render_to_string", render_to_string);
    NODE_SET_PROTOTYPE_METHOD(constructor, "render_to_file", render_to_file);
    NODE_SET_PROTOTYPE_METHOD(constructor, "scaleDenominator", scale_denominator);

    // layer access
    NODE_SET_PROTOTYPE_METHOD(constructor, "add_layer", add_layer);
    NODE_SET_PROTOTYPE_METHOD(constructor, "get_layer", get_layer);

    // temp hack to expose layer metadata
    NODE_SET_PROTOTYPE_METHOD(constructor, "layers", layers);
    NODE_SET_PROTOTYPE_METHOD(constructor, "features", features);
    NODE_SET_PROTOTYPE_METHOD(constructor, "describe_data", describe_data);

    // properties
    ATTR(constructor, "srs", get_prop, set_prop);

    target->Set(String::NewSymbol("Map"),constructor->GetFunction());
    //eio_set_max_poll_reqs(10);
    //eio_set_min_parallel(10);
}

Map::Map(int width, int height) :
  ObjectWrap(),
  map_(new mapnik::Map(width,height)) {}

Map::Map(int width, int height, std::string const& srs) :
  ObjectWrap(),
  map_(new mapnik::Map(width,height,srs)) {}

Map::~Map()
{
    // std::clog << "~Map(node)\n";
    // release is handled by boost::shared_ptr
}

Handle<Value> Map::New(const Arguments& args)
{
    HandleScope scope;

    if (!args.IsConstructCall())
        return ThrowException(String::New("Cannot call constructor as function, you need to use 'new' keyword"));

    // accept a reference or v8:External?
    if (args[0]->IsExternal())
    {
        return ThrowException(String::New("No support yet for passing v8:External wrapper around C++ void*"));
    }

    if (args.Length() == 2)
    {
        if (!args[0]->IsNumber() || !args[1]->IsNumber())
            return ThrowException(Exception::Error(
               String::New("'width' and 'height' must be a integers")));
        Map* m = new Map(args[0]->IntegerValue(),args[1]->IntegerValue());
        m->Wrap(args.This());
        return args.This();
    }
    else if (args.Length() == 3)
    {
        if (!args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsString())
            return ThrowException(Exception::Error(
               String::New("'width' and 'height' must be a integers")));
        Map* m = new Map(args[0]->IntegerValue(),args[1]->IntegerValue(),TOSTR(args[2]));
        m->Wrap(args.This());
        return args.This();
    }
    else
    {
        return ThrowException(Exception::Error(
          String::New("please provide Map width and height and optional srs")));
    }
    //return args.This();
    return Undefined();
}

Handle<Value> Map::get_prop(Local<String> property,
                         const AccessorInfo& info)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(info.This());
    std::string a = TOSTR(property);
    if (a == "srs")
        return scope.Close(String::New(m->map_->srs().c_str()));
    return Undefined();
}

void Map::set_prop(Local<String> property,
                         Local<Value> value,
                         const AccessorInfo& info)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(info.Holder());
    std::string a = TOSTR(property);
    if (a == "srs")
    {
        if (!value->IsString()) {
            ThrowException(Exception::Error(
               String::New("'srs' must be a string")));
        } else {
            m->map_->set_srs(TOSTR(value));
        }
    }

}

Handle<Value> Map::add_layer(const Arguments &args) {
    HandleScope scope;
    Local<Object> obj = args[0]->ToObject();
    if (args[0]->IsNull() || args[0]->IsUndefined() || !Layer::constructor->HasInstance(obj))
      return ThrowException(Exception::TypeError(String::New("mapnik.Layer expected")));
    Layer *l = ObjectWrap::Unwrap<Layer>(obj);
    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    // TODO - addLayer should be add_layer in mapnik
    m->map_->addLayer(*l->get());
    return Undefined();
}

Handle<Value> Map::get_layer(const Arguments& args)
{
    HandleScope scope;

    if (!args.Length() == 1)
      return ThrowException(Exception::Error(
        String::New("Please provide layer index")));

    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("layer index must be an integer")));

    unsigned index = args[0]->IntegerValue();

    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    std::vector<mapnik::layer> & layers = m->map_->layers();

    // TODO - we don't know features.length at this point
    if ( index < layers.size())
    {
        //mapnik::layer & lay_ref = layers[index];
        return scope.Close(Layer::New(layers[index]));
    }
    else
    {
      return ThrowException(Exception::TypeError(
        String::New("invalid layer index")));
    }
    return Undefined();

}


Handle<Value> Map::layers(const Arguments& args)
{
    HandleScope scope;

    // todo - optimize by allowing indexing...
    /*if (!args.Length() == 1)
      return ThrowException(Exception::Error(
        String::New("Please provide layer index")));

    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("layer index must be an integer")));
    */

    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    std::vector<mapnik::layer> const & layers = m->map_->layers();
    Local<Array> a = Array::New(layers.size());

    for (unsigned i = 0; i < layers.size(); ++i )
    {
        const mapnik::layer & layer = layers[i];
        Local<Object> meta = Object::New();
        layer_as_json(meta,layer);
        a->Set(i, meta);
    }

    return scope.Close(a);

}

Handle<Value> Map::describe_data(const Arguments& args)
{
    HandleScope scope;

    // todo - optimize by allowing indexing...
    /*if (!args.Length() == 1)
      return ThrowException(Exception::Error(
        String::New("Please provide layer index")));

    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("layer index must be an integer")));
    */

    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    std::vector<mapnik::layer> const & layers = m->map_->layers();

    Local<Object> meta = Object::New();

    for (unsigned i = 0; i < layers.size(); ++i )
    {
        const mapnik::layer & layer = layers[i];
        Local<Object> description = Object::New();
        mapnik::datasource_ptr ds = layer.datasource();
        if (ds)
        {
            describe_datasource(description,ds);
        }
        meta->Set(String::NewSymbol(layer.name().c_str()), description);
    }

    return scope.Close(meta);

}


Handle<Value> Map::features(const Arguments& args)
{
    HandleScope scope;

    if (!args.Length() >= 1)
      return ThrowException(Exception::Error(
        String::New("Please provide layer index")));

    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("layer index must be an integer")));

    unsigned first = 0;
    unsigned last = 0;

    // we are slicing
    if (args.Length() == 3)
    {
        if (!args[1]->IsNumber() || !args[2]->IsNumber())
            return ThrowException(Exception::Error(
               String::New("Index of 'first' and 'last' feature must be an integer")));
        first = args[1]->IntegerValue();
        last = args[2]->IntegerValue();
    }

    unsigned index = args[0]->IntegerValue();

    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    std::vector<mapnik::layer> const & layers = m->map_->layers();

    // TODO - we don't know features.length at this point
    Local<Array> a = Array::New(0);
    if ( index < layers.size())
    {
        mapnik::layer const& layer = layers[index];
        mapnik::datasource_ptr ds = layer.datasource();
        if (ds)
        {
            datasource_features(a,ds,first,last);
        }
    }

    return scope.Close(a);

}

Handle<Value> Map::clear(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    m->map_->remove_all();
    return Undefined();
}

Handle<Value> Map::resize(const Arguments& args)
{
    HandleScope scope;

    if (!args.Length() == 2)
      return ThrowException(Exception::Error(
        String::New("Please provide width and height")));

    if (!args[0]->IsNumber() || !args[1]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("width and height must be integers")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    m->map_->resize(args[0]->IntegerValue(),args[1]->IntegerValue());
    return Undefined();
}


Handle<Value> Map::width(const Arguments& args)
{
    HandleScope scope;
    if (!args.Length() == 0)
      return ThrowException(Exception::Error(
        String::New("accepts no arguments")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    Local<Value> width = Integer::New(m->map_->width());
    return scope.Close(width);
}

Handle<Value> Map::height(const Arguments& args)
{
    HandleScope scope;
    if (!args.Length() == 0)
      return ThrowException(Exception::Error(
        String::New("accepts no arguments")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    Local<Value> width = Integer::New(m->map_->height());
    return scope.Close(width);
}

Handle<Value> Map::buffer_size(const Arguments& args)
{
    HandleScope scope;
    if (!args.Length() == 1)
      return ThrowException(Exception::Error(
        String::New("Please provide a buffer_size")));

    if (!args[0]->IsNumber())
      return ThrowException(Exception::TypeError(
        String::New("buffer_size must be an integer")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    m->map_->set_buffer_size(args[0]->IntegerValue());
    return Undefined();
}

Handle<Value> Map::load(const Arguments& args)
{
    HandleScope scope;
    if (args.Length() != 1 || !args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("first argument must be a path to a mapnik stylesheet")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    std::string const& stylesheet = TOSTR(args[0]);
    bool strict = false;
    try
    {
        mapnik::load_map(*m->map_,stylesheet,strict);
    }
    catch (const mapnik::config_error & ex )
    {
      return ThrowException(Exception::Error(
        String::New(ex.what())));
    }
    catch (...)
    {
      return ThrowException(Exception::TypeError(
        String::New("something went wrong loading the map")));
    }
    return Undefined();
}

Handle<Value> Map::save(const Arguments& args)
{
    HandleScope scope;
    if (args.Length() != 1 || !args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("first argument must be a path to map.xml to save")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    std::string const& filename = TOSTR(args[0]);
    bool explicit_defaults = false;
    mapnik::save_map(*m->map_,filename,explicit_defaults);
    return Undefined();
}

Handle<Value> Map::from_string(const Arguments& args)
{
    HandleScope scope;
    if (!args.Length() >= 1) {
        return ThrowException(Exception::TypeError(
        String::New("Accepts 2 arguments: map string and base_url")));
    }

    if (!args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("first argument must be a mapnik stylesheet string")));

    if (!args[1]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("second argument must be a base_url to interpret any relative path from")));

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    std::string const& stylesheet = TOSTR(args[0]);
    bool strict = false;
    std::string const& base_url = TOSTR(args[1]);
    try
    {
        mapnik::load_map_string(*m->map_,stylesheet,strict,base_url);
    }
    catch (const mapnik::config_error & ex )
    {
      return ThrowException(Exception::Error(
        String::New(ex.what())));
    }
    catch (...)
    {
      return ThrowException(Exception::TypeError(
        String::New("something went wrong loading the map")));
    }
    return Undefined();
}

Handle<Value> Map::to_string(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    bool explicit_defaults = false;
    std::string map_string = mapnik::save_map_to_string(*m->map_,explicit_defaults);
    return scope.Close(String::New(map_string.c_str()));
}

Handle<Value> Map::scale_denominator(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    return scope.Close(Number::New(m->map_->scale_denominator()));
}

Handle<Value> Map::extent(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    Local<Array> a = Array::New(4);
    mapnik::box2d<double> e = m->map_->get_current_extent();
    a->Set(0, Number::New(e.minx()));
    a->Set(1, Number::New(e.miny()));
    a->Set(2, Number::New(e.maxx()));
    a->Set(3, Number::New(e.maxy()));
    return scope.Close(a);
}

Handle<Value> Map::zoom_all(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    m->map_->zoom_all();
    return Undefined();
}

Handle<Value> Map::zoom_to_box(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    double minx;
    double miny;
    double maxx;
    double maxy;

    if (args.Length() == 1)
    {
        if (!args[0]->IsArray())
            return ThrowException(Exception::Error(
               String::New("Must provide an array of: [minx,miny,maxx,maxy]")));
        Local<Array> a = Local<Array>::Cast(args[0]);
        minx = a->Get(0)->NumberValue();
        miny = a->Get(1)->NumberValue();
        maxx = a->Get(2)->NumberValue();
        maxy = a->Get(3)->NumberValue();

    }
    else if (args.Length() != 4)
      return ThrowException(Exception::Error(
        String::New("Must provide 4 arguments: minx,miny,maxx,maxy")));
    else {
        minx = args[0]->NumberValue();
        miny = args[1]->NumberValue();
        maxx = args[2]->NumberValue();
        maxy = args[3]->NumberValue();
    }
    mapnik::box2d<double> box(minx,miny,maxx,maxy);
    m->map_->zoom_to_box(box);
    return Undefined();
}

typedef struct {
    Map *m;
    std::string format;
    mapnik::box2d<double> bbox;
    bool error;
    std::string error_name;
    std::string im_string;
    Persistent<Function> cb;
} closure_t;

Handle<Value> Map::render(const Arguments& args)
{
    HandleScope scope;

    /*
    std::clog << "eio_nreqs" << eio_nreqs() << "\n";
    std::clog << "eio_nready" << eio_nready() << "\n";
    std::clog << "eio_npending" << eio_npending() << "\n";
    std::clog << "eio_nthreads" << eio_nthreads() << "\n";
    */

    if (args.Length() < 3)
        return ThrowException(Exception::TypeError(
          String::New("requires three arguments, a extent array, a format, and a callback")));

    // extent array
    if (!args[0]->IsArray())
        return ThrowException(Exception::TypeError(
           String::New("first argument must be an extent array of: [minx,miny,maxx,maxy]")));

    // format
    if (!args[1]->IsString())
        return ThrowException(Exception::TypeError(
           String::New("second argument must be an format string")));

    // function callback
    if (!args[args.Length()-1]->IsFunction())
        return ThrowException(Exception::TypeError(
                  String::New("last argument must be a callback function")));

    Local<Array> a = Local<Array>::Cast(args[0]);
    uint32_t a_length = a->Length();
    if (!a_length  == 4) {
        return ThrowException(Exception::TypeError(
           String::New("first argument must be 4 item array of: [minx,miny,maxx,maxy]")));
    }

    closure_t *closure = new closure_t();

    if (!closure) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate enough memory")));
    }

    double minx = a->Get(0)->NumberValue();
    double miny = a->Get(1)->NumberValue();
    double maxx = a->Get(2)->NumberValue();
    double maxy = a->Get(3)->NumberValue();

    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    closure->m = m;
    closure->format = TOSTR(args[1]);
    closure->error = false;
    closure->bbox = mapnik::box2d<double>(minx,miny,maxx,maxy);
    closure->cb = Persistent<Function>::New(Handle<Function>::Cast(args[args.Length()-1]));
    eio_custom(EIO_Render, EIO_PRI_DEFAULT, EIO_AfterRender, closure);
    ev_ref(EV_DEFAULT_UC);
    m->Ref();
    return Undefined();
}

int Map::EIO_Render(eio_req *req)
{
    closure_t *closure = static_cast<closure_t *>(req->data);

    // zoom to
    closure->m->map_->zoom_to_box(closure->bbox);
    try
    {
        mapnik::image_32 im(closure->m->map_->width(),closure->m->map_->height());
        mapnik::agg_renderer<mapnik::image_32> ren(*closure->m->map_,im);
        ren.apply();
        closure->im_string = save_to_string(im, closure->format);
    }
    catch (const mapnik::config_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::datasource_exception & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::proj_init_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const std::runtime_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::ImageWriterException & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const std::exception & ex)
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (...)
    {
        closure->error = true;
        closure->error_name = "unknown exception happened while rendering the map,\n this should not happen, please submit a bug report";
    }
    return 0;
}

int Map::EIO_AfterRender(eio_req *req)
{
    HandleScope scope;

    closure_t *closure = static_cast<closure_t *>(req->data);
    ev_unref(EV_DEFAULT_UC);

    TryCatch try_catch;

    if (closure->error) {
        // TODO - add more attributes
        // https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error
        Local<Value> argv[1] = { Exception::Error(String::New(closure->error_name.c_str())) };
        closure->cb->Call(Context::GetCurrent()->Global(), 1, argv);
    } else {
        #if NODE_VERSION_AT_LEAST(0,3,0)
          node::Buffer *retbuf = Buffer::New((char *)closure->im_string.data(),closure->im_string.size());
        #else
          node::Buffer *retbuf = Buffer::New(closure->im_string.size());
          memcpy(retbuf->data(), closure->im_string.data(), closure->im_string.size());
        #endif
        Local<Value> argv[2] = { Local<Value>::New(Null()), Local<Value>::New(retbuf->handle_) };
        closure->cb->Call(Context::GetCurrent()->Global(), 2, argv);
    }

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    closure->m->Unref();
    closure->cb.Dispose();
    delete closure;
    return 0;
}

Handle<Value> Map::render_to_string(const Arguments& args)
{
    HandleScope scope;

    if (!args.Length() >= 1 || !args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("argument must be a format string")));

    std::string format = TOSTR(args[0]);

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    std::string s;
    try
    {
        mapnik::image_32 im(m->map_->width(),m->map_->height());
        mapnik::agg_renderer<mapnik::image_32> ren(*m->map_,im);
        ren.apply();
        //std::string ss = mapnik::save_to_string<mapnik::image_data_32>(im.data(),"png");
        s = save_to_string(im, format);

    }
    catch (const mapnik::config_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::datasource_exception & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::proj_init_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const std::runtime_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::ImageWriterException & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (std::exception & ex)
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (...)
    {
        return ThrowException(Exception::TypeError(
          String::New("unknown exception happened while rendering the map, please submit a bug report")));
    }

    #if NODE_VERSION_AT_LEAST(0,3,0)
      node::Buffer *retbuf = Buffer::New((char*)s.data(),s.size());
    #else
      node::Buffer *retbuf = Buffer::New(s.size());
      memcpy(retbuf->data(), s.data(), s.size());
    #endif

    return scope.Close(retbuf->handle_);
}

Handle<Value> Map::render_to_file(const Arguments& args)
{
    HandleScope scope;
    if (!args.Length() >= 1 || !args[0]->IsString())
      return ThrowException(Exception::TypeError(
        String::New("first argument must be a path to a file to save")));

    if (args.Length() > 2)
      return ThrowException(Exception::TypeError(
        String::New("accepts two arguments, a required path to a file, and an optional options object, eg. {format: 'pdf'}")));

    std::string format("");

    if (args.Length() == 2){
      if (!args[1]->IsObject())
        return ThrowException(Exception::TypeError(
          String::New("second argument is optional, but if provided must be an object, eg. {format: 'pdf'}")));

        Local<Object> options = args[1]->ToObject();
        if (options->Has(String::New("format")))
        {
            Local<Value> format_opt = options->Get(String::New("format"));
            if (!format_opt->IsString())
              return ThrowException(Exception::TypeError(
                String::New("'format' must be a String")));

            format = TOSTR(format_opt);
        }
    }

    Map* m = ObjectWrap::Unwrap<Map>(args.This());
    std::string const& output = TOSTR(args[0]);

    if (format.empty()) {
        format = mapnik::guess_type(output);
        if (format == "<unknown>") {
            std::ostringstream s("");
            s << "unknown output extension for: " << output << "\n";
            return ThrowException(Exception::Error(
                String::New(s.str().c_str())));
        }
    }

    try
    {

        if (format == "pdf" || format == "svg" || format =="ps" || format == "ARGB32" || format == "RGB24")
        {
    #if defined(HAVE_CAIRO)
            mapnik::save_to_cairo_file(*m->map_,output,format);
    #else
            std::ostringstream s("");
            s << "Cairo backend is not available, cannot write to " << format << "\n";
            return ThrowException(Exception::Error(
              String::New(s.str().c_str())));
    #endif
        }
        else
        {
            mapnik::image_32 im(m->map_->width(),m->map_->height());
            mapnik::agg_renderer<mapnik::image_32> ren(*m->map_,im);
            ren.apply();
            mapnik::save_to_file<mapnik::image_data_32>(im.data(),output);
        }
    }
    catch (const mapnik::config_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::datasource_exception & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::proj_init_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const std::runtime_error & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (const mapnik::ImageWriterException & ex )
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (std::exception & ex)
    {
        return ThrowException(Exception::Error(
          String::New(ex.what())));
    }
    catch (...)
    {
        return ThrowException(Exception::TypeError(
          String::New("unknown exception happened while rendering the map, please submit a bug report")));
    }
    return Undefined();
}


Handle<Value> Map::render_grid(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    if (args.Length() != 3)
      return ThrowException(Exception::Error(
        String::New("please provide layer idx, step, join_field")));

    if ((!args[0]->IsNumber() || !args[1]->IsNumber()))
        return ThrowException(Exception::TypeError(
           String::New("layer idx and step must be integers")));

    if ((!args[2]->IsString()))
        return ThrowException(Exception::TypeError(
           String::New("layer join_field must be a string")));

    std:: size_t layer_idx = static_cast<std::size_t>(args[0]->NumberValue());
    unsigned int step = args[1]->NumberValue();
    std::string join_field = TOSTR(args[2]);


    std::vector<mapnik::layer> const& layers = m->map_->layers();
    std::size_t layer_num = layers.size();

    if (layer_idx >= layer_num) {
        std::ostringstream s;
        s << "Zero-based layer index '" << layer_idx << "' not valid, only '"
          << layers.size() << "' layers are in map";
        return ThrowException(Exception::TypeError(
           String::New(s.str().c_str())));
    }

    mapnik::layer const& layer = layers[layer_idx];

    double z = 0;
    mapnik::CoordTransform tr = m->map_->view_transform();
    const mapnik::box2d<double>&  e = m->map_->get_current_extent();
    double minx = e.minx();
    double miny = e.miny();
    double maxx = e.maxx();
    double maxy = e.maxy();
    mapnik::projection dest(m->map_->srs());
    mapnik::projection source(layer.srs());
    mapnik::proj_transform prj_trans(source,dest);
    prj_trans.backward(minx,miny,z);
    prj_trans.backward(maxx,maxy,z);
    mapnik::datasource_ptr ds = layer.datasource();
    mapnik::box2d<double> bbox = mapnik::box2d<double>(minx,miny,maxx,maxy);
    #if MAPNIK_VERSION >= 800
        mapnik::query q(bbox);
    #else
        mapnik::query q(bbox,1.0,1.0);
    #endif
    q.add_property_name(join_field);
    mapnik::featureset_ptr fs = ds->features(q);
    typedef mapnik::coord_transform2<mapnik::CoordTransform,mapnik::geometry_type> path_type;


    agg::grid_value feature_id = 1;
    std::map<agg::grid_value, std::string> feature_keys;
    std::map<agg::grid_value, std::string>::const_iterator feature_pos;

    std::map<std::string, UChar> keys;
    std::map<std::string, UChar>::const_iterator key_pos;
    std::vector<std::string> key_order;
    UChar codepoint = 31;
    unsigned int length = 256 / step;
    unsigned int len = length * (length + 3) + 1;
    UnicodeString::UnicodeString str(len, 0, len);

    if (fs)
    {

        unsigned int width = m->map_->width();
        unsigned int height = m->map_->width();
        agg::grid_value* buf = new agg::grid_value[width * height];
        agg::grid_rendering_buffer renbuf(buf, width, height, width);
        agg::grid_renderer<agg::span_grid> ren_grid(renbuf);
        agg::grid_rasterizer ras_grid;
        
        agg::grid_value no_hit = 0;
        std::string no_hit_val = "";
        feature_keys[no_hit] = no_hit_val;
        ren_grid.clear(no_hit);

        mapnik::feature_ptr feature;
        while ((feature = fs->next()))
        {
            ras_grid.reset();
            ++feature_id;
            
            for (unsigned i=0;i<feature->num_geometries();++i)
            {
                mapnik::geometry_type const& geom=feature->get_geometry(i);
                //if (geom.num_points() > 2)
                //{
                    path_type path(tr,geom,prj_trans);

                    ras_grid.add_path(path);
                //}
            }

            std::string val = "";
            std::map<std::string,mapnik::value> const& fprops = feature->props();
            std::map<std::string,mapnik::value>::const_iterator const& itr = fprops.find(join_field);
            if (itr != fprops.end())
            {
                val = itr->second.to_string();
            }

            feature_pos = feature_keys.find(feature_id);
            if (feature_pos == feature_keys.end())
            {
                feature_keys[feature_id] = val;
            }
            
            ras_grid.render(ren_grid, feature_id);

        }
        
        // resample and utf-ize the grid
        int32_t index = 0;
        str.setCharAt(index++, (UChar)'[');
        for (unsigned y = 0; y < renbuf.height(); y=y+step)
        {
            str.setCharAt(index++, (UChar)'"');
            agg::grid_value* row = renbuf.row(y);
            for (unsigned x = 0; x < renbuf.width(); x=x+step)
            {
                agg::grid_value id = row[x];
                feature_pos = feature_keys.find(id);
                if (feature_pos != feature_keys.end())
                {
                    std::string val = feature_pos->second;
                    key_pos = keys.find(val);
                    if (key_pos == keys.end())
                    {
                        // Create a new entry for this key. Skip the codepoints that
                        // can't be encoded directly in JSON.
                        ++codepoint;
                        if (codepoint == 34) ++codepoint;      // Skip "
                        else if (codepoint == 92) ++codepoint; // Skip backslash
                    
                        keys[val] = codepoint;
                        key_order.push_back(val);
                        str.setCharAt(index++, codepoint);
                    }
                    else
                    {
                        str.setCharAt(index++, key_pos->second);
                    }

                }
                // else, shouldn't get here...
            }
            str.setCharAt(index++, (UChar)'"');
            str.setCharAt(index++, (UChar)',');
        }
        str.setCharAt(index - 1, (UChar)']');

        delete buf;
    }


    Local<Array> keys_a = Array::New(keys.size());
    std::vector<std::string>::iterator it;
    unsigned int i;
    for (it = key_order.begin(), i = 0; it < key_order.end(); ++it, ++i)
    {
        keys_a->Set(i, String::New((*it).c_str()));
    }


    Local<Object> json = Object::New();
    json->Set(String::NewSymbol("grid"), String::New(str.getBuffer(), len));
    json->Set(String::NewSymbol("keys"), keys_a);

    return scope.Close(json);
}


typedef struct {
    Map *m;
    std::size_t layer_idx;
    unsigned int step;
    std::string join_field;
    bool error;
    std::string error_name;
    Persistent<Object> json;
    Persistent<Function> cb;
} generate_hit_grid_t;

Handle<Value> Map::generate_hit_grid(const Arguments& args)
{
    HandleScope scope;
    Map* m = ObjectWrap::Unwrap<Map>(args.This());

    if (args.Length() != 4)
      return ThrowException(Exception::Error(
        String::New("please provide layer idx, step, join_field, and callback")));

    if ((!args[0]->IsNumber() || !args[1]->IsNumber()))
        return ThrowException(Exception::TypeError(
           String::New("layer idx and step must be integers")));

    if ((!args[2]->IsString()))
        return ThrowException(Exception::TypeError(
           String::New("layer join_field must be a string")));

    // function callback
    if (!args[3]->IsFunction())
        return ThrowException(Exception::TypeError(
                  String::New("fourth argument must be a callback function")));

    generate_hit_grid_t *closure = new generate_hit_grid_t();

    if (!closure) {
    V8::LowMemoryNotification();
    return ThrowException(Exception::Error(
        String::New("Could not allocate enough memory")));
    }

    closure->m = m;
    closure->layer_idx = static_cast<std::size_t>(args[0]->NumberValue());
    closure->step = args[1]->NumberValue();
    closure->join_field = TOSTR(args[2]);
    closure->error = false;
    closure->cb = Persistent<Function>::New(Handle<Function>::Cast(args[3]));

    eio_custom(EIO_GenerateHitGrid, EIO_PRI_DEFAULT, EIO_AfterGenerateHitGrid, closure);
    ev_ref(EV_DEFAULT_UC);
    m->Ref();
    return Undefined();
}

int Map::EIO_GenerateHitGrid(eio_req *req)
{
    generate_hit_grid_t *closure = static_cast<generate_hit_grid_t *>(req->data);

    Map* m = closure->m;
    std::size_t layer_idx = closure->layer_idx;
    unsigned int step = closure->step;
    std::string  const& join_field = closure->join_field;

    unsigned int tile_size = m->map_->width();

    UChar codepoint = 31; // Last ASCII control char.
    unsigned int length = 256 / step;

    // The exact string length:
    //   +3: length + two quotes and a comma
    //   +1: we don't need the last comma, but we need [ and ]
    unsigned int len = length * (length + 3) + 1;

    UnicodeString::UnicodeString str(len, 0, len);

    std::map<std::string, UChar> keys;
    std::map<std::string, UChar>::const_iterator pos;
    std::vector<std::string> key_order;

    std::vector<mapnik::layer> const& layers = m->map_->layers();
    std::size_t layer_num = layers.size();

    if (layer_idx >= layer_num) {
        std::ostringstream s;
        s << "Zero-based layer index '" << layer_idx << "' not valid, only '"
          << layers.size() << "' layers are in map";
        closure->error = true;
        closure->error_name = s.str();
        return 0;
    }


    try
    {
        int32_t index = 0;
        str.setCharAt(index++, (UChar)'[');
        for (unsigned y=0;y<tile_size;y=y+step)
        {
            str.setCharAt(index++, (UChar)'"');
            for (unsigned x=0;x<tile_size;x=x+step)
            {
                mapnik::featureset_ptr fs_hit = m->map_->query_map_point(layer_idx,x,y);

                std::string val = "";

                if (fs_hit)
                {
                    mapnik::feature_ptr fp = fs_hit->next();
                    if (fp)
                    {
                        std::map<std::string,mapnik::value> const& fprops = fp->props();
                        std::map<std::string,mapnik::value>::const_iterator const& itr = fprops.find(join_field);
                        if (itr != fprops.end())
                        {
                            val = itr->second.to_string();
                            //a->Set(x+y, String::New((const char*)itr->second.to_string().c_str()));
                        }
                        else
                        {
                            closure->error = true;
                            closure->error_name = "Invalid key!";
                            return 0;
                        }

                    }
                }

                // Find out the UChar value associated with the val
                // If it doesn't exist, create a new one and add it to the map
                pos = keys.find(val);
                if (pos == keys.end())
                {
                    // Create a new entry for this key. Skip the codepoints that
                    // can't be encoded directly in JSON.
                    ++codepoint;
                    if (codepoint == 34) ++codepoint;      // Skip "
                    else if (codepoint == 92) ++codepoint; // Skip backslash

                    keys[val] = codepoint;
                    key_order.push_back(val);
                    str.setCharAt(index++, codepoint);
                }
                else
                {
                    str.setCharAt(index++, pos->second);
                }

            }
            str.setCharAt(index++, (UChar)'"');
            str.setCharAt(index++, (UChar)',');
        }
        // Overwrite the last comma.
        str.setCharAt(index - 1, (UChar)']');
    }
    catch (const mapnik::config_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::datasource_exception & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::proj_init_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const std::runtime_error & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const mapnik::ImageWriterException & ex )
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (const std::exception & ex)
    {
        closure->error = true;
        closure->error_name = ex.what();
    }
    catch (...)
    {
        closure->error = true;
        closure->error_name = "Unknown error occured, please file bug";
    }

    if (!closure->error)
    {
        HandleScope scope;

        // Create the key array.
        Local<Array> keys_a = Array::New(keys.size());
        std::vector<std::string>::iterator it;
        unsigned int i;
        for (it = key_order.begin(), i = 0; it < key_order.end(); ++it, ++i)
        {
            keys_a->Set(i, String::New((*it).c_str()));
        }

        // Create the return hash.
        closure->json = Persistent<Object>::New(Object::New());
        closure->json->Set(String::NewSymbol("grid"), String::New(str.getBuffer(), len));
        closure->json->Set(String::NewSymbol("keys"), keys_a);
    }

    return 0;
}

int Map::EIO_AfterGenerateHitGrid(eio_req *req)
{
    HandleScope scope;

    generate_hit_grid_t *closure = static_cast<generate_hit_grid_t *>(req->data);
    ev_unref(EV_DEFAULT_UC);

    TryCatch try_catch;

    if (closure->error) {
        // TODO - add more attributes
        // https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error
        Local<Value> argv[1] = { Exception::Error(String::New(closure->error_name.c_str())) };
        closure->cb->Call(Context::GetCurrent()->Global(), 1, argv);
    } else {
        Local<Value> argv[2] = { Local<Value>::New(Null()), Local<Value>::New(closure->json) };
        closure->cb->Call(Context::GetCurrent()->Global(), 2, argv);
    }

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    closure->m->Unref();
    closure->cb.Dispose();
    closure->json.Dispose();
    delete closure;
    return 0;
}
