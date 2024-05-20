#pragma once

template <class F>
class Defer {
public:
    Defer(F&& f) : f_(f) {}
    ~Defer() { f_(); }
private:
    F f_;
};
