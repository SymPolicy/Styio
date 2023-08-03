#ifndef STYIO_UTILITY_H_
#define STYIO_UTILITY_H_

bool is_binary_token (int& cur_char)
{
  switch (cur_char)
  {
  case '+':
    return true;

  case '-':
    return true;

  case '*':
    return true;

  case '/':
    return true;

  case '%':
    return true;
  
  default:
    return false;
  }
}

std::string read_styio_file(const char* filename)
{
  if (std::filesystem::exists(filename))
  {
    std::ifstream file(filename);
    std::string contents;

    std::string str;
    while (std::getline(file, str))
    {
      contents += str;
      contents.push_back('\n');
    }

    contents += EOF;

    return contents;
  }

  return std::string("...");
}

std::string read_styio_file(std::filesystem::path filename)
{
  if (std::filesystem::exists(filename))
  {
    std::ifstream file(filename);
    std::string contents;

    std::string str;
    while (std::getline(file, str))
    {
      contents += str;
      contents.push_back('\n');
    }

    contents += EOF;

    return contents;
  }

  return std::string("...");
}

#endif