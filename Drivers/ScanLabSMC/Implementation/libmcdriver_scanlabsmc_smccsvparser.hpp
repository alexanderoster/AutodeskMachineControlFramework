#ifndef __LIBMCDRIVER_SCANLABSMC_SMCCSVPARSER
#define __LIBMCDRIVER_SCANLABSMC_SMCCSVPARSER

#include <string>
#include <vector>
#include <functional>



namespace LibMCDriver_ScanLabSMC {
namespace Impl {

    /**
        * @brief A fast single-pass CSV parser that loads the file into memory
        *        and parses fields without copying using pointer slicing.
        */
    class CSMCCSVParser {
    public:

        /**
            * @brief Enumeration of supported field parser types.
            * Used for mapping column definitions to parser functions.
            */
        enum class FieldParserType {
            /**
                * Skips this field during parsing. Equivalent to nullptr in targets.
                * Useful for ignoring unused CSV columns.
                */
            None,
            /**
                * Parses a field as a timestamp using CSMCCSVParser::ParseDouble.
                * Expected format: "+0.000000000", "-12.345678901"
                * Typically represents absolute or relative time in seconds.
                */
            Timestamp,

            /**
                * Parses a field as signed integer using CSMCCSVParser::ParseInt.
                * Expects input like: "42", "-7", "0"
                */
            Int,

            /**
                * Parses a field as unsigned 32-bit integer using CSMCCSVParser::ParseUInt32.
                * Expects input like: "123", "0", "4294967295"
                */
            UInt32,

            /**
                * Parses a field as double using CSMCCSVParser::ParseDouble.
                * Expects fixed-point or scientific notation: "3.14", "+0.0", "-1.23e-5"
                */
            Double,

            /**
                * Parses a field as boolean using CSMCCSVParser::ParseBool.
                * Accepts "1", "0", "true", "false", "yes", "no"
                */
            Bool,

            /**
                * Parses a field as std::string using CSMCCSVParser::ParseString.
                * Accepts arbitrary UTF-8 text.
                */
            String,

            /**
                * Parses a field as a laser signal descriptor using CSMCCSVParser::ParseLaserSignal.
                * Expects compound format: "1_1_+2.0_+3.5_+8.1"
                */
            LaserSignal
        };

        /**
         * @brief Defines the processing steps that are applied to a field after parsing from CSV.
         *
         * Each field may undergo one or more of the following processing steps:
         * - Nop: No post-processing is applied. The parsed value is stored as-is.
         * - Extend: If a field has fewer entries than timestamps, the last known value is extended.
         * - Interpolate: Missing values between known data points are interpolated linearly.
         */
        enum FieldProcessingStep : uint8_t {
            /**
             * @brief No additional processing; values are stored as parsed.
             */
            Nop = 0,

            /**
             * @brief Repeat the last known value to align vector length with timestamps.
             */
            Extend = 1,

            /**
             * @brief Perform linear interpolation between known values to fill in gaps.
             */
            Interpolate = 2
        };

           

        /**
         * @brief Metadata describing how a specific CSV field should be interpreted and processed.
         *
         * This structure pairs a field's parser type with the processing steps that should
         * be applied after parsing. Used to define the parsing behavior for each column.
         */
        struct FieldMetadata {
            /**
             * @brief Defines how the field should be parsed (e.g., as double, int, bool, etc.).
             */
            FieldParserType         type;
            /**
             * @brief Specifies post-processing steps such as Extend or Interpolate.
             */
            uint8_t                 processing;
        };

        /**
         * @brief Associates metadata with a pointer to the destination buffer for parsed data.
         *
         * This structure maps a single CSV field to both its interpretation rules (metadata)
         * and the memory location where the parsed values should be stored.
         */
        struct FieldBinding {
            /**
             * @brief Describes how the field should be parsed and post-processed.
             */
            FieldMetadata meta;

            /**
             * @brief Pointer to the target container where parsed values are appended.
             *
             * This is typically a pointer to a std::vector<T>.
             */
            void* target;
        };


        /**
            * @brief Type alias for parser function pointer.
            * The function receives:
            * - pointer to character buffer (not null-terminated),
            * - length of the field,
            * - pointer to target vector<T>.
            */
        using ParserFunc = std::function<void(const char*, size_t, void*, void*)>;

        /**
         * @brief Function type used to extend a data vector after parsing.
         *
         * This function is responsible for duplicating the last known value
         * to ensure the vector matches the length of the timestamp vector.
         *
         * @param target Pointer to the target data vector (e.g., std::vector<T>).
         * @param ts_target Pointer to the timestamp vector (std::vector<double>).
         */
        using ExtenderFunc = std::function<void(void*, void*)>;
        

        /**
         * @brief Function type used to interpolate values between two known timestamp indices.
         *
         * This function is applied when a field supports interpolation between sparse data points.
         * It modifies the target vector in-place to insert estimated values using linear interpolation.
         *
         * @param idx_from Index of the first known timestamp.
         * @param idx_to Index of the second known timestamp.
         * @param target Pointer to the target data vector (e.g., std::vector<T>).
         * @param ts_target Pointer to the timestamp vector (std::vector<double>).
         */
        using InterpolatorFunc = std::function<void(size_t, size_t, void*, void*)>;

        /**
            * @brief Constructs the parser and loads the entire file into memory.
            * @param filename Path to the CSV file.
            * @param delimiter Delimiter character used in the CSV (e.g. ',', ';').
            */
        CSMCCSVParser(const std::string& filename, char delimiter);

        /**
            * @brief Starts parsing the CSV file using user-provided parsers and target vectors.
            * @param field_bindings A vector of binding between fields metadata and destination targe containers
            */
        void Parse(const std::vector<FieldBinding>& field_bindings);

        // Static parsing functions for specific data types

        static constexpr size_t TimestampDefaultInc = 10;

        /**
         * @brief Adds a timestamp to the timestamp target vector.
         * @param ts_target Pointer to the timestamp vector (std::vector<double>*).
         * @param ts Timestamp value to be added.
         */
        static void PushTimestamp(void* ts_target, double ts);

        /**
         * @brief Returns the last timestamp from the timestamp target vector.
         * @param ts_target Pointer to the timestamp vector (std::vector<double>*).
         * @return The last timestamp value or 0.0 if the vector is empty.
         */
        static double LastTimestamp(void* ts_target);


        /**
         * @brief Parses a timestamp string and stores it in the provided target.
         * @param data Pointer to the character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output target (std::time_t*).
         * @param ts_target Optional pointer to the timestamp vector.
         */
        static void ParseTimestamp(const char* data, size_t length, void* target, void* ts_target);

        /**
         * @brief Parses a boolean value (e.g. '1', 'true') and stores it in the target vector.
         * @param data Pointer to the character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output vector (std::vector<bool>*).
         * @param ts_target Optional pointer to the timestamp vector.
         */
        static void ParseBool(const char* data, size_t length, void* target, void* ts_target);

        /**
         * @brief Parses a string and stores it in the target vector.
         * @param data Pointer to the character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output vector (std::vector<std::string>*).
         * @param ts_target Optional pointer to the timestamp vector.
         */
        static void ParseString(const char* data, size_t length, void* target, void* ts_target);

        /**
         * @brief Generic value parser for numeric types (int, uint32_t, double).
         * @tparam T The numeric type to parse.
         * @param data Pointer to the character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output vector (std::vector<T>*).
         * @param ts_target Optional pointer to the timestamp vector.
         */
        template<typename T>
        static void ParseValue(const char* data, size_t length, void* target, void* ts_target);

        /**
         * @brief Fills the vector with repeated last value to match the length of the timestamp vector.
         * @tparam T The value type.
         * @param target Pointer to the output vector (std::vector<T>*).
         * @param ts_target Pointer to the timestamp vector (std::vector<double>*).
         */
        template<typename T>
        static void ExtendVector(void* target, void* ts_target);

        /**
         * @brief Interpolates values between two known points in time.
         * @tparam T The value type.
         * @param idx_from Start index in the timestamp vector.
         * @param idx_to End index in the timestamp vector.
         * @param target Pointer to the output vector (std::vector<T>*).
         * @param ts_target Pointer to the timestamp vector (std::vector<double>*).
         */
        template<typename T>
        static void InterpolateVector(size_t idx_from, size_t idx_to, void* target, void* ts_target);

        /**
         * @brief Parses a complex laser signal structure and stores toggle states in the target vector.
         * @param data Pointer to the character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output vector (std::vector<bool>*).
         * @param ts_target Pointer to the timestamp vector (std::vector<double>*).
         */
        static void ParseLaserSignal(const char* data, size_t length, void* target, void* ts_target);


        /**
         * @brief Formats a double with fixed precision and explicit sign.
         * @param value Double value to format.
         * @return Formatted string representation of the double.
         */
        static std::string FormatDouble(double value);

    private:
        /**
            * @brief Handles comment lines (starting with '#').
            * This function can be customized to log, ignore or store comments.
            */
        void ParseComment(const char* lineStart, size_t length);

        /**
         * @brief Retrieves the appropriate parser function for a given field type.
         * @param type The type of the field to be parsed.
         * @return A function pointer to the corresponding parser.
         */
        static CSMCCSVParser::ParserFunc GetParser(FieldParserType type);

        /**
         * @brief Retrieves the extender function for the given field type.
         * @param type The type of the field to extend.
         * @return A function pointer to the corresponding extender.
         */
        static CSMCCSVParser::ExtenderFunc GetExtender(FieldParserType type);

        /**
         * @brief Retrieves the interpolation function for the given field type.
         * @param type The type of the field to interpolate.
         * @return A function pointer to the corresponding interpolator.
         */
        static CSMCCSVParser::InterpolatorFunc GetInterpolator(FieldParserType type);

        /**
         * @brief Represents a laser toggle sub-cycle with timing.
         */
        struct SubCycle {
            bool Toggle;        ///< Toggle state (on/off).
            double TimeOffset;  ///< Time offset from the base timestamp.
        };

        /**
         * @brief Internal parser for complex LaserSignal field with sub-cycle toggle encoding.
         * @param data Pointer to the input character buffer.
         * @param length Length of the buffer.
         * @param target Pointer to the output vector of SubCycle.
         * @param ts_target Pointer to the timestamp vector.
         */
        static void ParseLaserSignal_Internal(const char* data, size_t length, void* target, void* ts_target);

        std::vector<char> m_fileBuff;   ///< Raw file contents held in memory
        char m_delimiter;               ///< CSV field delimiter character
        size_t m_rowCount = 0;          ///< Number of parsed data rows
    };

} //namespace Impl 
} //namespace LibMCDriver_ScanLabSMC 
    

#endif // __LIBMCDRIVER_SCANLABSMC_SMCCSVPARSER