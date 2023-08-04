bool is_binary_token (char& cur_char)
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