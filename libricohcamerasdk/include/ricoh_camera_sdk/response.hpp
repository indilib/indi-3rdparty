// Copyright (c) 2017 Ricoh Company, Ltd. All Rights Reserved.
#ifndef RICOH_CAMERA_SDK_RESPONSE_HPP_
#define RICOH_CAMERA_SDK_RESPONSE_HPP_

#include <memory>
#include <vector>

#include <ricoh_camera_sdk/export.hpp>

namespace Ricoh {
namespace CameraController {

/**
 * @brief The type of a result responded form a camera device.
 */
enum class Result {
    Ok,
    Error
};

class Capture;
class Error;

/**
 * @brief The class for a response from a camera device.
 */
class RCSDK_API Response {
public:
    /**
     * @brief Constructs with the specified result.
     *
     * @param result The result
     */
    Response(Result result);

    /**
     * @brief Constructs with the specified result and error.
     *
     * @param result The result
     * @param error  The error
     */
    Response(Result result, const std::shared_ptr<Error>& error);

    virtual ~Response() = default;

    /**
     * @brief Returns the result of request to
     *
     * @return The result
     */
    Result getResult() const;

    /**
     * @brief Returns the errors
     *
     * @return The errors
     */
    const std::vector<std::shared_ptr<Error>>& getErrors() const;

    /**
     * @brief Adds the error
     *
     * @param error The error
     */
    void addError(const std::shared_ptr<Error>& error);

protected:
    Result result_;
    std::vector<std::shared_ptr<Error>> errors_;
};

/**
 * @brief The class for a response having a capture from a camera device.
 */
class RCSDK_API StartCaptureResponse : public Response {
public:

    /**
     * @brief Constructs with the specified result.
     *
     * @param result The result
     */
    StartCaptureResponse(Result result);

    /**
     * @brief Constructs with the specified result and error.
     *
     * @param result The result
     * @param error  The error
     */
    StartCaptureResponse(Result result, const std::shared_ptr<Error>& error);

    /**
     * @brief Constructs with Result::Ok and the specified capture.
     *
     * @param capture The capture
     */
    StartCaptureResponse(const std::shared_ptr<const Capture>& capture);

    ~StartCaptureResponse() = default;

    /**
     * @brief Returns the capture.
     *
     * @return The capture
     */
    std::shared_ptr<const Capture> getCapture() const;

private:
    std::shared_ptr<const Capture> capture_;
};

} // namespace CameraController
} // namespace Ricoh

#endif // RICOH_CAMERA_SDK_RESPONSE_HPP_
