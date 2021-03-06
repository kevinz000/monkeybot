#ifndef CONFIG_H_
#define CONFIG_H_

#include "Common.h"

#include <map>
#include <string>
#include <tchar.h>

namespace Convert {
    template <typename T>
    T Get(const tstring& val);
}

class Config {
public:
    typedef std::map<tstring, tstring>::const_iterator const_iterator;

private:
    std::map<tstring, tstring> m_Variables;

public:
    bool Load(const tstring& filename);

    template <typename T>
    T Get(const tstring& var) const {
        tstring out;
        out.resize(var.length());
        for (unsigned int i = 0; i < var.length(); i++)
            out[i] = _totlower(var[i]);

        auto found = m_Variables.find(out);

        if (found == m_Variables.end()) return T();

        return Convert::Get<T>(found->second);
    }

    void Set(const tstring& var, const tstring& val);

    const_iterator begin() const { return m_Variables.begin(); }
    const_iterator end() const { return m_Variables.end(); }
};

#endif
