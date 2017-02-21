//Fractal viewer utils
#ifndef FractalUtil__
#define FractalUtil__

#define rgba(r,g,b,a) (r|(g<<8)|(b<<16)|(a*255<<24))

extern const char *fractalVertexShader;
extern const char *fractalFragmentShader;

class FractalShader : public Shader
{
private:

public:
  FractalShader(std::string& fsrc)
  {
    init(fractalVertexShader, fsrc);
    //Save vertex attribute locations
    vertexPositionAttribute = glGetAttribLocation(program, "aVertexPosition");
    glEnableVertexAttribArray(vertexPositionAttribute);
  }
  FractalShader()
  {
    init(fractalVertexShader, fractalFragmentShader);
    //Save vertex attribute locations
    vertexPositionAttribute = glGetAttribLocation(program, "aVertexPosition");
    glEnableVertexAttribArray(vertexPositionAttribute);
  }

  GLint vertexPositionAttribute;
};

//Utility class for parsing fractal property,value strings
class FractalParser
{
  std::map<std::string,std::string> props;

public:
  FractalParser() {};

  FractalParser(std::istream& is, char delim)
  {
    parse(is, delim);
  }
  FractalParser(std::istream& is)
  {
    parse(is);
  }

  //Parse lines with delimiter, ie: key=value
  void parse(std::istream& is, char delim)
  {
    std::string line;
    while(std::getline(is, line))
    {
      std::istringstream iss(line);
      std::string key, value;

      std::getline(iss, key, delim);
      std::getline(iss, value, delim);

      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
      props[key] = value;
      //std::size_t uniformp = key.find("@@");
      //if (uniformp != std::string::npos)
      //std::cerr << "Key " << key << " == " << value << std::endl;
    }
  }

  //Parse lines as whitespace separated eg: " key   value"
  void parse(std::istream& is)
  {
    std::string line;
    while(std::getline(is, line))
    {
      std::istringstream iss(line);
      std::string key, value;

      iss >> key;
      iss >> value;

      std::transform(key.begin(), key.end(), key.begin(), ::tolower);
      props[key] = value;
    }
  }

  std::string get(std::string key)
  {
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    if (props.find(key) != props.end()) return props[key];
    return "";
  }

  std::string operator[] (std::string key)
  {
    return get(key);
  }

  int Int(std::string key, int def=0)
  {
    std::stringstream parsess(get(key));
    int val;
    if (!(parsess >> val)) return def;
    return val;
  }

  float Float(std::string key, float def=0.0)
  {
    std::stringstream parsess(get(key));
    float val;
    if (!(parsess >> val)) return def;
    return val;
  }

  bool Float2(std::string key, float val[2])
  {
    std::string float2 = get(key);
    //Check valid
    std::size_t pos = float2.find(",");
    if (pos == std::string::npos) return false;
    //Remove ()
    std::stringstream parsess(float2.substr(1, float2.length()-1));
    parsess >> val[0];
    parsess.ignore(2, ',');
    parsess >> val[1];
    return true;
  }

  bool Bool(std::string key, bool def=false)
  {
    std::stringstream parsess(get(key));
    bool val;
    //Accepts 0/1
    parsess >> val;
    if (parsess.fail())
    {
      parsess.clear();
      //Accepts true/false
      parsess >> std::boolalpha >> val;
      if (parsess.fail()) val = def;
    }
    //std::cerr << "key: " << key << " = " << get(key) << " => " << val << std::endl;
    return val;
  }

  int RGBA(std::string key)
  {
    std::string value = get(key);
    if (value.length() == 0) return 0;
    Colour c(value);
    return c.value;
  }
};

#endif
