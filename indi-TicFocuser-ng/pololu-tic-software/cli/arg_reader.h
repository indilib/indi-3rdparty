#pragma once

class arg_reader
{
public:
  explicit arg_reader(int argc, char ** argv) : argc(argc), argv(argv), index(0)
  {
  }

  arg_reader() : argc(0), argv(NULL), index(0)
  {
  }

  // Advance and return the current argument, or NULL if we have reached the
  // end.  The first call of this returns argv[1] because argv[0] is just the
  // program name.
  const char * next()
  {
    if (index < argc)
    {
      index++;
      return argv[index];
    }
    else
    {
      return NULL;
    }
  }

  // Return the argument before the current one, or NULL.
  const char * last() const
  {
    if (index > 0)
    {
      return argv[index - 1];
    }
    else
    {
      return NULL;
    }
  }

private:
  int argc;
  char ** argv;
  int index;
};
