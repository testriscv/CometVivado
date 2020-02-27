template <int x> struct log2const {
  enum { value = 1 + log2const<x / 2>::value };
};

template <> struct log2const<1> {
  enum { value = 0 };
};
