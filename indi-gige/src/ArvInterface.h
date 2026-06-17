/*
 GigE interface wrapper on aravis
 Copyright (C) 2016 Hendrik Beijeman (hbeyeman@gmail.com)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef CPP_ARV_IFACE_H
#define CPP_ARV_IFACE_H

#include <cstdint>
#include <cstddef>

#include <string>
#include <utility>
#include <memory>
#include <iterator>
#include <functional>

namespace arv
{
typedef enum {
    ARV_EXPOSURE_UNKNOWN  = -1, //!< Unknown status?
    ARV_EXPOSURE_FINISHED = 0,  //!< Exposure has finished, image returned
    ARV_EXPOSURE_BUSY,          //!< Exposure is still ungoing
    ARV_EXPOSURE_FILLING,       //!< Exposure is finished, being transferred on the lower layer
    ARV_EXPOSURE_FAILED,        //!< Exposure has finished, with an error, no image returned.

} ARV_EXPOSURE_STATUS;

template <class T>
class min_max_property
{
  public:
    min_max_property() : _min(0), _max(0), _val(0), _incr(0) {}
    min_max_property(T const min, T const max, T const val, T const incr = T(0))
    {
        this->_min = min;
        this->_max = max;
        this->_val = val;
        this->_incr = incr;
    }

    void update(T const min, T const max)
    {
        this->_min = min;
        this->_max = max;
    }

    void set_increment(T const incr)
    {
        this->_incr = incr;
    }

    /** Set a new value for the property, obeying max/min limits and following
     * incremental restrictions if set (i.e. if not 0). */
    void set(T new_val)
    {
        if (this->_incr != T(0))
            // enforce alignment to increments
            new_val = int((new_val - this->_min)/this->_incr) * this->_incr + this->_min;

        if (new_val > this->_max)
            this->_val = this->_max;
        else if (new_val < this->_min)
            this->_val = this->_min;
        else
            this->_val = new_val;
    }
    void set_single(T const single_val) { this->_min = this->_max = this->_val = single_val; }

    T val() { return this->_val; }
    T min() { return this->_min; }
    T max() { return this->_max; }
    T incr() { return this->_incr; }

  private:
    T _min, _max, _val, _incr;
};

class ArvCamera
{
  public:
    typedef std::function<void(uint8_t const *const, size_t)> HandleImgCB;

    ArvCamera([[maybe_unused]] std::string device_id,
              [[maybe_unused]] std::string model_name) {}
    virtual ~ArvCamera() = default;
    virtual bool connect()            = 0;
    virtual bool disconnect()         = 0;
    virtual bool is_connected()       = 0;
    /** Is a single-frame acquisition is underway? */
    virtual bool is_exposing_single() = 0;
    /** Is a multiple-frame acquisition (streaming) is underway? */
    virtual bool is_streaming()       = 0;
    bool is_acquiring() {
      return this->is_exposing_single() || this->is_streaming();
    }

    /* Get properties */
    virtual const char *vendor_name()                  = 0;
    virtual const char *model_name()                   = 0;
    virtual const char *device_id()                    = 0;
    virtual int get_frame_byte_size()                  = 0;
    virtual min_max_property<int> get_bin_x()          = 0;
    virtual min_max_property<int> get_bin_y()          = 0;
    virtual min_max_property<int> get_x_offset()       = 0;
    virtual min_max_property<int> get_y_offset()       = 0;
    virtual min_max_property<int> get_width()          = 0;
    virtual min_max_property<int> get_height()         = 0;
    virtual min_max_property<int> get_bpp()            = 0;
    virtual min_max_property<double> get_pixel_pitch() = 0;
    virtual min_max_property<double> get_exposure()    = 0;
    virtual min_max_property<double> get_gain()        = 0;
    virtual min_max_property<double> get_frame_rate()  = 0;
    virtual bool has_feature(const char * feature)     = 0;
    virtual double get_float(const char * feature)     = 0;

    /* Set geometry */
    virtual void set_bin(int const bin_x, int const bin_y)                        = 0;
    /** update this class information of binning from the camera hardware.
     * @return pair of binning values for x and y.
     */
    virtual std::pair<int,int> update_bin(void)                                   = 0;
    virtual void set_geometry(int const x, int const y, int const w, int const h) = 0;
    /** update this class information of geometry from the camera hardware.
     * @return tuple of (x, y, w, h).
     */
    virtual std::tuple<int,int,int,int> update_geometry(void)                     = 0;

    /* Set exposure */
    virtual void set_exposure_time(double const val) = 0;
    virtual void set_gain(double const val)          = 0;

    /** Start a single-frame acquisition. */
    virtual void exposure_start(void)                      = 0;
    virtual void exposure_abort(void)                      = 0;
    virtual ARV_EXPOSURE_STATUS exposure_poll(HandleImgCB fn_image_callback) = 0;
    virtual ARV_EXPOSURE_STATUS next_streaming_image(HandleImgCB fn_image_callback) = 0;
    virtual void stop_streaming(void)                      = 0;
    /** Start a multi-frame acquisition (i.e. streaming). */
    void start_streaming(unsigned int n_buffers=10) {
      this->start_streaming_impl(n_buffers);
    }

  protected:
    virtual void start_streaming_impl(unsigned int n_buffers)   = 0;
};

class ArvFactory
{
  public:
    /** Find, instantiate, and return a unique_ptr of the first available
     * ArvCamera.
     */
    static std::unique_ptr<ArvCamera> find_first_available(void);

    /** Camera device index, used by the camera iterator and to instantiate an
     * ArvCamera.
     */
    class Index {
        int index;
      public:
        Index(int index) : index(index) { }

        /** Attempt to create a camera instance for the camera index.
         * Does *not* update the device list.
         */
        std::unique_ptr<ArvCamera> instantiate() const;

        Index & operator++() {
          ++this->index;
          return *this;
        }
        bool operator==(const Index & other) const {
          return this->index == other.index;
        }
        bool operator!=(const Index & other) const {
          return this->index != other.index;
        }
    };

    /** Camera index iterator. */
    class Iterator {
        Index index;
    public:
        // 1. Mandatory STL iterator traits
        using iterator_category = std::input_iterator_tag;
        using value_type        = Index;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const Index*;
        using reference         = const Index&;

        Iterator(Index index) : index(index) { }

        /** Dereference operator. */
        reference operator*() const {
            return index;
        }

        /** Prefix increment operator. */
        Iterator& operator++() {
            ++this->index;
            return *this;
        }

        // Postfix increment operator
        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        // Comparison operators
        bool operator!=(const Iterator& other) const {
            return this->index != other.index;
        }

        bool operator==(const Iterator& other) const {
            return this->index == other.index;
        }
    };

    /** Updates the list of devices and returns the iterator to the first. */
    Iterator begin();
    /** Returns an iterator to the currently known end.
     * This function does *not* update the list of devices.
     */
    Iterator end();
};

} /* Namepsace */

#endif
