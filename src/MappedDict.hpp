#pragma once

/**
 * @brief A simple class to deal with 1-1 value mapping, a la python dictionaries
 * @details Operations operate on pointers so that this class has no data storage,
 * data should be set up by the caller and passed in to the constructor.
 * @tparam KeyType Type that should be the 'key'
 * @tparam ValueType Type that should be the 'value'
 */
template <class KeyType, class ValueType> class MappedDict
{
  public:
    typedef struct {
        KeyType in;
        ValueType out;
    } DictEntry_t;

    MappedDict(DictEntry_t *dictPtr, size_t dictSize) : _dictPtr(dictPtr), _dictSize(dictSize)
    {
    }

    /**
     * Try to retrieve a value from the mapping
     * @param[in] query Key to look for in the dictionary
     * @param[out] rtnValPtr Pointer to store the return value in, if successful
     * @return true if the query was found in the dictionary, false otherwise
     */
    bool tryGet(KeyType query, ValueType *rtnValPtr)
    {
        bool foundElement = false;
        for (unsigned i = 0; i < _dictSize; i++)
        {
            if (_dictPtr[i].in == query)
            {
                foundElement = true;
                *rtnValPtr   = _dictPtr[i].out;
                break;
            }
        }
        return foundElement;
    }

  private:
    DictEntry_t *_dictPtr;  ///< Pointer to dictionary storage
    size_t _dictSize;       ///< Number of elements in the dictionary
};
