#pragma once

#include <optional>
#include <unistd.h>

namespace Config{
    static const uint16_t Nodesize = 256;
};

typedef uint64_t Key;
typedef uint64_t Value;
typedef std::pair<void *, uint64_t> Metainformation;


struct IndexNode {
    uint32_t accesses = 0;
    virtual bool is_inner() const = 0;
    virtual IndexNode *get_right_sibling() = 0;
    virtual void set_right_sibling(IndexNode *) = 0;
};


class Index {
    public:
    virtual ~Index() {};
    virtual std::optional<Value> get(const Key key, bool linear_search, bool get_node_path = false) = 0;
    virtual void put(const Key key, Value value) = 0;
    virtual std::vector<Metainformation> get_node_path() = 0;
    virtual std::vector<Metainformation> get_metainformation() = 0;
    virtual IndexNode *get_root() const = 0;
    virtual void set_root(IndexNode *new_root, bool delete_old_root = true) = 0;
};