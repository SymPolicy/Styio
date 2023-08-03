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