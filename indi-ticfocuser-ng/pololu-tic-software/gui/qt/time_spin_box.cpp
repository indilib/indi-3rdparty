#include "time_spin_box.h"

time_spin_box::time_spin_box(QWidget * parent) : QSpinBox(parent)
{
    connect(this, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &time_spin_box::set_code_from_value);
}

void time_spin_box::set_code_from_value(int value)
{
    code = value;
}

void time_spin_box::set_mapping(const QMap<int, int> &new_mapping)
{
    if (mapping == new_mapping)
    {
        return;
    }
    mapping = new_mapping;
    if (!mapping.empty())
    {
        setRange(mapping.firstKey(), mapping.lastKey());
    }
    setValue(code);
}

QString time_spin_box::text_from_value(int value) const
{
    return QString::number(value / 1000.0, 'f', decimals);
}

// Finds a key that corresponds to the specified text.  If there are multiple
// keys, returns the canonical one: the one with the lowest nanosecond value,
// or the one with the lowest key value if there is a tie.
int time_spin_box::canonical_key_for_text(const QString &text) const
{
    bool found_key = false;
    int best_key = 0;
    int best_value = 0;
    for (auto i = mapping.constBegin(); i != mapping.constEnd(); ++i)
    {
        if (text == text_from_value(i.value()) &&
                (!found_key || i.value() < best_value ||
                 (i.value() == best_value && i.key() < best_key)))
        {
            found_key = true;
            best_key = i.key();
            best_value = i.value();
        }
    }
    return best_key;
}

// First, tries to select a key that:
// a) has a milliamp value greater than the specified key's
// b) has a text different from the specified key's
// while optimizing for:
// c) lowest possible milliamp value
//
// If it cannot find something satisfying a && b, it means we are near the top
// of the mapping, so instead it selects the key specified as an argument.
//
// Finally, it passes the key through canonical_key_for_text to make sure there
// is no surprising hidden state.
int time_spin_box::step_up(int key) const
{
    if (mapping.empty())
    {
        return key + 1;
    }

    int value = mapping.value(key, 0);
    QString value_text = textFromValue(key);

    bool found_key = false;
    int best_value = std::numeric_limits<int>::max();
    for (auto i = mapping.constBegin(); i != mapping.constEnd(); ++i)
    {
        if (i.value() > value && i.value() < best_value &&
                value_text != text_from_value(i.value()))
        {
            found_key = true;
            best_value = i.value();
        }
    }
    if (!found_key)
    {
        best_value = value;
    }

    return canonical_key_for_text(text_from_value(best_value));
}

// Equivalent to step_up but steps down.
int time_spin_box::step_down(int key) const
{
    if (mapping.empty())
    {
        return key - 1;
    }

    int value = mapping.value(key, 0);
    QString value_text = textFromValue(key);

    bool found_key = false;
    int best_value = std::numeric_limits<int>::min();
    for (auto i = mapping.constBegin(); i != mapping.constEnd(); ++i)
    {
        if (i.value() < value && i.value() > best_value &&
                value_text != text_from_value(i.value()))
        {
            found_key = true;
            best_value = i.value();
        }
    }
    if (!found_key)
    {
        best_value = value;
    }

    return canonical_key_for_text(text_from_value(best_value));
}

void time_spin_box::stepBy(int step_value)
{
    int code = value();

    while (step_value > 0)
    {
        code = step_up(code);
        step_value--;
    }

    while (step_value < 0)
    {
        code = step_down(code);
        step_value++;
    }

    setValue(code);
    selectAll();

    fflush(stdout);
}

// Converts user input into a value (time_spin_box::value()).
int time_spin_box::valueFromText(const QString &text) const
{
    QString copy = text.toUpper();

    bool entered_ns = copy.contains("N");
    copy.remove(QRegularExpression("[^0-9.]"));
    double entered_double = copy.toDouble();
    if (!entered_ns)
    {
        entered_double *= 1000;
    }
    int entered_value = qRound(entered_double);

    if (mapping.empty())
    {
        return entered_value;
    }

    int best_value = std::numeric_limits<int>::min();
    for (auto i = mapping.constBegin(); i != mapping.constEnd(); ++i)
    {
        if (i.value() <= entered_value && i.value() > best_value)
        {
            best_value = i.value();
        }
    }
    return canonical_key_for_text(text_from_value(best_value));
}

// Determines how values are displayed to the user.
QString time_spin_box::textFromValue(int value) const
{
    if (mapping.empty())
    {
        return text_from_value(value);
    }
    return text_from_value(mapping.value(value));
}

// Allow the user to input letters as well as digits, but restrict the number
// of digits which can be entered and the allowed letters.
QValidator::State time_spin_box::validate(QString &input, int &pos) const
{
    (void)pos;
    QRegularExpression r("\\A\\d{0,6}(\\.\\d{0,3})?\\s*n?s?\\s*\u00b5?s?\\Z",
                         QRegularExpression::CaseInsensitiveOption);

    if (input.isEmpty())
    {
        return QValidator::Intermediate;
    }

    if (r.match(input).hasMatch())
    {
        return QValidator::Acceptable;
    }
    else
    {
        return QValidator::Invalid;
    }
}
