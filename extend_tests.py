import os
import re

def pad_with_zeros(n: int):
  return '{:0>8}'.format(n)

def create_file(filename, content=""):
  try:
    with open(filename, 'w') as file:
      file.write(content)
  except Exception as e:
    print(f"Exception while creating '{filename}': {e}")

def extend_tests(dir_path, n=10):
  files = list(filter(
    lambda x: os.path.isfile(f"{dir_path}/{x}") & x.endswith(".styio"), 
    sorted(os.listdir(dir_path), reverse=True)
  ))

  match = re.match(r'^(\d+)\.styio$', files[0])

  if match:
    latest = int(match.group(1))
    fnames = [f'{dir_path}/{pad_with_zeros(i)}.styio' for i in range(latest+1, latest+n+1)]
    for fn in fnames:
      if "parsing" in fn:
        create_file(fn, r"// RUN: /home/Styio/styio --styio-ast --file %s | /usr/bin/FileCheck %s")
      elif "lowering" in fn:
        create_file(fn, r"// RUN: /home/Styio/styio --styio-ir --file %s | /usr/bin/FileCheck %s")
      elif "codegen" in fn:
        create_file(fn, r"// RUN: /home/Styio/styio --llvm-ir --file %s | /usr/bin/FileCheck %s")
      else:
        create_file(fn, r"")
  else:
    print(files)
    print("No match.")


if __name__ == "__main__":
  # tests_path = os.path.dirname(os.path.realpath(__file__))
  # for dir_name in os.listdir(tests_path):
  #   print(f"{file_path}/{dir_name}")
  #   print(os.path.isdir(f"{file_path}/{dir_name}"))
  extend_tests("/home/Styio/tests/parsing/forward", 5)