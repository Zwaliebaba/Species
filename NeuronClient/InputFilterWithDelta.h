#pragma once

#include <memory>
#include <vector>

#include "InputFilterSpec.h"
#include "InputFilter.h"


class InputFilterWithDelta : public InputFilter
{
  private:
    std::vector<std::unique_ptr<const InputFilterSpec>> m_specs;
    std::vector<std::unique_ptr<InputDetails>> m_oldDetails;
    std::vector<std::unique_ptr<InputDetails>> m_details;

  protected:
    void registerDeltaID(InputFilterSpec& spec);

    InputDetails const& getOldDetails(filterspec_id_t id) const;
    InputDetails const& getOldDetails(InputFilterSpec const& spec) const;

    InputDetails const& getDetails(filterspec_id_t id) const;
    InputDetails const& getDetails(InputFilterSpec const& spec) const;

    // Subclasses must implement this
    virtual void calcDetails(InputFilterSpec const& spec, InputDetails& details) = 0;

    // Preferably override the first of these, unless you need to use more
    // filterspec information, in which case you should probably override both
    virtual int getDelta(filterspec_id_t id, InputDetails& delta) const;
    virtual int getDelta(InputFilterSpec const& spec, InputDetails& delta) const;

    // After this, the current details will be invalid
    void ageDetails();

  public:
    virtual void Advance();
};
