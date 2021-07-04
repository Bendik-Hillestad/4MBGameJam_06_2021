/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
*/

/*
┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                                                     │
│ Author: Bendik Hillestad                                                                                            │
│                                                                                                                     │
│ This is my entry to the 4MB Game Jam 06/2021 (https://itch.io/jam/4mb).                                             │
|                                                                                                                     |
│ Although unnecessary, I have chosen to avoid the CRT completely, including the Standard Library, and without using  |
| any workarounds like dynamically loading the CRT and getting the functions I need. Similarly, I am avoiding         |
| exceptions, dynamic allocations, floating-point math and third-party libraries. Some of these choices may increase  |
| the size of the executable, so they were not made exclusively to reduce size, although exceptions were naturally    |
| avoided for this reason. Mainly, this was a personal challenge.                                                     |
│                                                                                                                     │
│ To build this game, you will need "Build Tools for Visual Studio 2019" (This comes with the Visual Studio IDE).     │
│   Link: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019                            │
│ A full installation of Visual Studio is not required. Once you have this, you must open a "Developer Command Prompt │
│ for VS 2019", then navigate to the project directory and execute build.bat. The resulting executable should be in   │
│ the "out/" folder. Although this game can be compiled as a 64-bit executable, the build script will be producing a  |
| 32-bit executable. If any compiler errors are encountered, check first if your build tools are up-to-date. If they  |
| are, please report this issue to me on GitHub.                                                                      |
│                                                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
*/

//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// Setting up some macros.                                                                                            │
//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

#if !defined(_DEBUG) && !defined(NDEBUG)
    #define NDEBUG
#elif defined(_DEBUG) && defined(NDEBUG)
    #error both _DEBUG and NDEBUG defined
#endif

#if !defined(_M_AMD64) && !defined(_M_IX86)
    #error unsupported platform
#elif defined(_M_AMD64) && defined(_M_IX86)
    #error both _M_AMD64 and _M_IX86 defined
#endif

#define G21_STRINGIFY_IMPL(x) #x
#define G21_STRINGIFY(x) G21_STRINGIFY_IMPL(x)

//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// Including required headers.                                                                                        │
//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

#define _WIN32_WINNT _WIN32_WINNT_WINXP // In theory our code could run on Windows XP if compiled with a supporting 
                                        // toolset, however Windows 7 SP1 is generally the oldest we can target without
                                        // extra work. It is also unlikely to find a genuine Windows XP machine with a
                                        // a recent enough graphics card to run our game, as mainstream support for
                                        // Windows XP ended in 2009, while OpenGL 4.3 came out in 2012. But whatever.
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <intrin.h>

#include <gl/gl.h>
#include <gl/glext.h>
#include <gl/wglext.h>

//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// Disabling some bad warnings.                                                                                       │
//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

#pragma warning(disable : 4615) // C4615: #pragma warning: unknown user warning type
#pragma warning(disable : 4201) // C4201: nonstandard extension used: nameless struct/union

//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
// Implementation of the game begins from here.                                                                       │
//────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

namespace // Wrap all our functions and globals in an anonymous namespace for internal linkage.
{
    // Setup our fixed-width integer types.

    using u8  = unsigned __int8;
    using u16 = unsigned __int16;
    using u32 = unsigned __int32;
    using u64 = unsigned __int64;
    using i8  =   signed __int8;
    using i16 =   signed __int16;
    using i32 =   signed __int32;
    using i64 =   signed __int64;

    // Determine the pointer size.

    #if defined(_M_AMD64)
        using usize = u64;
        using isize = i64;
    #elif defined(_M_IX86)
        using usize = u32;
        using isize = i32;
    #endif

    consteval usize operator ""_usize(u64 value)
    {
        return static_cast<usize>(value);
    }

    // Setup a very basic debug printing function.

    #ifdef _DEBUG
        HANDLE _g21_debug_out;

        template<usize N>
        void _g21_debug_print_impl(char const(&str)[N])
        {
            WriteConsoleA(_g21_debug_out, str, DWORD{ N - 1 }, nullptr, nullptr);
        }

        #define G21_DEBUG_INIT do { _g21_debug_out = GetStdHandle(STD_OUTPUT_HANDLE);  } while(0)
        #define G21_DEBUG_PRINT _g21_debug_print_impl 
    #else
        #define G21_DEBUG_INIT ((void)0)
        #define G21_DEBUG_PRINT __noop
    #endif

    // Setup some helper functions.

    // In a native 64-bit build, we just get a multiply by 60. Easy.
    // In an optimized 32-bit build, the compiler realises it can perform the multiplication by rewriting it as:
    //   ((x << 4) - x) << 2
    // which is relatively easy to perform with only 32-bit integers.
    // However, in an unoptimized 32-bit build, the compiler wants to use the CRT which is a big no-no. The solution is
    // to just implement the optimized 32-bit version ourselves with some inline-assembly for the 32-bit debug build.
    #if defined(_M_AMD64) || defined(NDEBUG)
    __forceinline u64 u64_multiply_by_60(u64 x)
    {
        return x * 60Ui64;
    }
    #else
    __declspec(naked) u64 u64_multiply_by_60(u64)
    {
        __asm
        {
            mov     eax, DWORD PTR [esp + 4]    // Loading the low and high part of x into registers.
            mov     edx, DWORD PTR [esp + 8]

            shld    edx, eax, 4                 // Logical left shift of 'x' by 4.
            shl     eax, 4                      //

            sub     eax, DWORD PTR [esp + 4]    // Subtract 'x' from the previous result.
            sbb     edx, DWORD PTR [esp + 8]    //

            shld    edx, eax, 2                 // Logical left shift of the previous result by 2.
            shl     eax, 2                      //

            ret     0                           // Return (result stored in eax and edx).
        }
    }
    #endif

    template<typename T, usize N>
    consteval usize countof(T const(&)[N])
    {
        return N;
    }

    template<typename T>
    constexpr T max(T a, T b)
    {
        return (a > b) ? a : b;
    }

    template<u32 X, u32 Multiple>
    struct round_up
    {
        static_assert((Multiple & (Multiple - 1Ui32)) == 0, "'Multiple' must be a power of two");

        static constexpr u32 value{ (X + (Multiple - 1Ui32)) & ~(Multiple - 1Ui32) };
    };

    template<u32 X, u32 Multiple>
    constexpr u32 round_up_v{ round_up<X, Multiple>::value };

    // Setup our templated vector types.

    template<typename T>
    concept SignedArithmetic = (static_cast<T>(-1) < static_cast<T>(0));

    template<typename T>
    struct vec2
    {
        union
        {
            struct { T x, y; };
            struct { T r, g; };
        };

        constexpr vec2() = default;

        explicit constexpr vec2(T v)
            : x{ v }, y{ v }
        {}

        explicit constexpr vec2(T x, T y)
            : x{ x }, y{ y }
        {}

        friend constexpr vec2 operator + (vec2 lhs, vec2 rhs)
        {
            return vec2{ lhs.x + rhs.x, lhs.y + rhs.y };
        }

        friend constexpr vec2 operator - (vec2 lhs, vec2 rhs)
        {
            return vec2{ lhs.x - rhs.x, lhs.y - rhs.y };
        }

        friend constexpr vec2 operator - (vec2 value) requires SignedArithmetic<T>
        {
            return vec2{ -value.x, -value.y };
        }
    };
    static_assert(__is_trivially_constructible(vec2<u8>));

    template<typename T>
    struct vec3
    {
        union
        {
            struct { T x, y, z; };
            struct { T r, g, b; };
        };

        constexpr vec3() = default;

        explicit constexpr vec3(T v)
            : x{ v }, y{ v }, z{ v }
        {}

        explicit constexpr vec3(T x, T y, T z)
            : x{ x }, y{ y }, z{ z }
        {}
    };
    static_assert(__is_trivially_constructible(vec3<u8>));

    template<typename T>
    struct vec4
    {
        union
        {
            struct { T x, y, z, w; };
            struct { T r, g, b, a; };
        };

        constexpr vec4() = default;

        explicit constexpr vec4(T v)
            : x{ v }, y{ v }, z{ v }, w{ v }
        {}

        explicit constexpr vec4(T x, T y, T z, T w)
            : x{ x }, y{ y }, z{ z }, w{ w }
        {}
    };
    static_assert(__is_trivially_constructible(vec4<u8>));

    // Setup various other helper types.

    template<typename T>
    struct array_view
    {
        T const* const data;
        usize    const size;

        template<usize N>
        constexpr array_view(T const(&arr)[N])
            : data{ arr },
              size{   N }
        {}

        constexpr T const* begin() const
        {
            return this->data;
        }

        constexpr T const* end() const
        {
            return this->data + this->size;
        }
    };

    template<typename T, usize Sz>
    struct array
    {
        using value_type = T;

        static constexpr usize size{ Sz };

        constexpr array() : data{} {}

        constexpr array(T const(&arr)[Sz])
        {
            for (usize i{ 0 }; i < Sz; ++i)
            {
                this->data[i] = arr[i];
            }
        }

        T data[Sz];
    };

    // Setup our fixed-point type.
    // This is fairly simple implementation which only implements the operations that are used in the game. It uses 1
    // bit for the sign, 15 bits for the integral part and 16 bits for the fractional part, and can trivially be copied
    // to the GPU.

    class fixed16_16
    {
        i32 _value;

        static constexpr i32 shift{ 16 };

    public:
        // Note, the constructor below needs to be explicitly defaulted, that means not omitted and not implemented
        // trivially like `constexpr fixed_16_16() : _value{} {}` (which would silence the warning) so that the class
        // qualifies as trivially default constructible (according to the C++ specification). This allows global arrays
        // of this type that are default constructed (either explicitly or implicitly) to be stored in the bss section,
        // meaning they occupy no space in the final executable. This is checked by a static assert further down.

        #pragma warning(disable : 26495) // C26495: '_value' is unitialized. // This is a false-positive
        constexpr fixed16_16() = default;
        #pragma warning(restore : 26495)

        explicit constexpr fixed16_16(i16 integer_value)
            : _value{ static_cast<i32>(static_cast<u32>(static_cast<i32>(integer_value)) << shift) }
        {}

        constexpr i32& raw()&
        {
            return _value;
        }

        constexpr i32 const& raw() const&
        {
            return _value;
        }

        friend constexpr i16 ifloor(fixed16_16 x)
        {
            return static_cast<i16>(x._value >> shift);
        }

        friend constexpr fixed16_16 floor(fixed16_16 x)
        {
            // Setup a mask that will clear the fractional component
            constexpr u32 mask{ ~((1Ui32 << shift) - 1Ui32) };

            fixed16_16 result;

            result._value = static_cast<i32>(static_cast<u32>(x._value) & mask);

            return result;
        }

        friend constexpr fixed16_16 fract(fixed16_16 x)
        {
            return (x - floor(x));
        }

        friend constexpr fixed16_16 operator + (fixed16_16 lhs, fixed16_16 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value + rhs._value;

            return result;
        }

        friend constexpr fixed16_16 operator + (fixed16_16 lhs, i16 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value + (static_cast<i32>(rhs) << shift);

            return result;
        }

        constexpr fixed16_16& operator += (fixed16_16 rhs)
        {
            this->_value += rhs._value;
            return *this;
        }

        constexpr fixed16_16& operator += (i16 rhs)
        {
            this->_value += static_cast<i32>(static_cast<u32>(static_cast<i32>(rhs)) << shift);
            return *this;
        }

        constexpr fixed16_16& operator -= (i16 rhs)
        {
            this->_value -= static_cast<i32>(static_cast<u32>(static_cast<i32>(rhs)) << shift);
            return *this;
        }

        friend constexpr fixed16_16 operator - (fixed16_16 rhs)
        {
            fixed16_16 result;

            result._value = -rhs._value;

            return result;
        }

        friend constexpr fixed16_16 operator - (fixed16_16 lhs, fixed16_16 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value - rhs._value;

            return result;
        }

        friend constexpr fixed16_16 operator - (fixed16_16 lhs, i16 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value - (static_cast<i32>(rhs) << shift);

            return result;
        }

        friend constexpr fixed16_16 operator * (fixed16_16 lhs, i32 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value * rhs;

            return result;
        }

        friend constexpr fixed16_16 operator / (fixed16_16 lhs, i32 rhs)
        {
            fixed16_16 result;

            result._value = lhs._value / rhs;

            return result;
        }

        friend constexpr bool operator > (fixed16_16 lhs, fixed16_16 rhs)
        {
            return (lhs._value > rhs._value);
        }

        friend constexpr bool operator != (fixed16_16 lhs, fixed16_16 rhs)
        {
            return (lhs._value != rhs._value);
        }

        static fixed16_16 sqrt(u16 value)
        {
            // This is a fairly classic software implementation of the square root using the Newton-Raphson method,
            // modified for fixed-point rather than floating-point numbers.

            // Rather than using 1 or (value / 2) as our initial guess for the algorithm, we instead first compute the
            // square root of two raised to the power of the integer binary logarithm of the input value. This gets us
            // very close to the square root of the input value, allowing the Newton-Raphson method to converge far
            // quicker. The beauty here is that this calculation simplifies into just computing two raised to the power
            // of half of the integer binary logarithm of the input value. The integer binary logarithm of the input
            // value is trivial to compute, as we only need to find the position of the most significant bit in the
            // binary representation of the number. Additionally, computing two raised to an integer power is as simple
            // as performing a logical left shift, eg two raised to the power of X is just 1 << X.
            // The formula looks something like this:
            //     initial guess = sqrt(pow(2, floor(log₂(value)))
            //                   = pow(2, floor(log₂(value)) / 2)
            //                   = 1 << (floor(log₂(value)) / 2)
            // For the division by 2 we round to the nearest integer by adding 1 before dividing.
            //
            // Because we do not care too much about precision, we only use two iterations of Newton-Raphson.

            // Early exit if the input value is 0
            if (value == 0) return fixed16_16{ 0 };

            // Calculate the integer logarithm of the input value by emitting the 'bsr' instruction. We could also use
            // the 'lzcnt' instruction, which is faster, however the size of the resulting assembly code is marginally
            // larger. Micro-optimizing the wrong things is what I love.
            u32 msb; (void)_BitScanReverse(reinterpret_cast<DWORD*>(&msb), value);

            // Calculate the initial guess
            u32 x{ (1Ui32 << ((msb + 1Ui32) >> 1)) << shift };

            // Perform two iterations of Newton-Raphson
            u32 const y{ static_cast<u32>(value) << shift };
            x = (x + (y / (x >> shift))) >> 1;
            x = (x + (y / (x >> shift))) >> 1;

            // Return the final result
            fixed16_16 result;
            result._value = x;
            return result;
        }
    };
    static_assert(__is_trivially_constructible(fixed16_16));

    // Setup our sprite vertex type.
    // This type wraps the data we send to the GPU during sprite rendering.

    struct sprite_vertex
    {
        vec2<fixed16_16> pos;
        u32 sprite_texture_index;
        u32 : 1; // Padding.
    };

    // Setup various configurable constants

    // The size in pixels of sprites. This metric is a bit weird since we render first to an internal buffer with a
    // fixed resolution and then we upscale that to the target display. So this does not represent the number of pixels
    // occupied on the target display. This value is mostly determined by how detailed the player's sprite is.
    constexpr u32 k_sprite_size{ 32 };

    // The per-frame acceleration due to gravity.
    constexpr fixed16_16 k_gravity{ fixed16_16{ 1020 } / (60*60) };

    // The maximum number of particles that may be active during a frame.
    constexpr u32 k_max_particle_count{ 1'000'000 };

    constexpr u32 k_sprites_vertices_per_quad{ 4 };
    constexpr u32 k_sprites_max_quad_count   { 128 }; // Maximum number of quads (sprites) drawn during a frame.
    constexpr u32 k_sprites_max_vertex_count { k_sprites_max_quad_count * k_sprites_vertices_per_quad };
    constexpr u32 k_sprites_indices_per_quad { 6 };
    constexpr u32 k_sprites_max_index_count  { k_sprites_max_quad_count * k_sprites_indices_per_quad };

    constexpr vec4<u8> k_sprite_palette[16]
    {
    /*0*/ vec4<u8>{   0,   0,   0,   0 }, // Transparent.
    /*1*/ vec4<u8>{   0,   0,   0, 255 }, // Solid black.
    /*2*/ vec4<u8>{ 230, 209, 188, 255 }, // Skin.
    /*3*/ vec4<u8>{ 228, 218, 153, 255 }, // Blonde hair.
    /*4*/ vec4<u8>{ 217, 200, 104, 255 }, // Blonde hair accent.
    /*5*/ vec4<u8>{ 208,  70,  72, 255 }, // Red coat.
    /*6*/ vec4<u8>{ 170,  51,  51, 255 }, // Red coat accent.
    /*7*/ vec4<u8>{  50, 101,  36, 255 }, // Green eyes.
    /*8*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*9*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*A*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*B*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*C*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*D*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*E*/ vec4<u8>{   0,   0,   0,   0 }, // Unused.
    /*F*/ vec4<u8>{   0,   0,   0,   0 }  // Unused.
    };

    constexpr u8 k_player_sprite[4][16][8]
    {
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
        },
        {
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x10, 0x11, 0x11, 0x01, 0x00, 0x00 }
        },
        {
            { 0x00, 0x10, 0x31, 0x33, 0x33, 0x13, 0x01, 0x00 },
            { 0x00, 0x10, 0x34, 0x33, 0x33, 0x33, 0x01, 0x00 },
            { 0x00, 0x10, 0x34, 0x33, 0x33, 0x33, 0x01, 0x00 },
            { 0x00, 0x41, 0x34, 0x22, 0x22, 0x22, 0x01, 0x00 },
            { 0x00, 0x41, 0x23, 0x22, 0x22, 0x22, 0x14, 0x00 },
            { 0x00, 0x41, 0x23, 0x77, 0x22, 0x72, 0x01, 0x00 },
            { 0x00, 0x11, 0x23, 0x77, 0x22, 0x72, 0x11, 0x01 },
            { 0x00, 0x31, 0x44, 0x22, 0x22, 0x22, 0x44, 0x01 },
            { 0x00, 0x41, 0x33, 0x33, 0x33, 0x33, 0x33, 0x01 },
            { 0x00, 0x10, 0x34, 0x33, 0x33, 0x33, 0x13, 0x00 },
            { 0x00, 0x10, 0x66, 0x34, 0x33, 0x33, 0x14, 0x00 },
            { 0x00, 0x10, 0x55, 0x45, 0x33, 0x43, 0x01, 0x00 },
            { 0x00, 0x10, 0x55, 0x55, 0x44, 0x54, 0x01, 0x00 },
            { 0x00, 0x10, 0x66, 0x11, 0x11, 0x66, 0x01, 0x00 },
            { 0x00, 0x10, 0x16, 0x00, 0x10, 0x16, 0x00, 0x00 },
            { 0x00, 0x10, 0x01, 0x00, 0x10, 0x01, 0x00, 0x00 }
        },
        {
            { 0x00, 0x10, 0x31, 0x33, 0x33, 0x13, 0x01, 0x00 },
            { 0x00, 0x10, 0x33, 0x33, 0x33, 0x43, 0x01, 0x00 },
            { 0x00, 0x10, 0x33, 0x33, 0x33, 0x43, 0x01, 0x00 },
            { 0x00, 0x10, 0x22, 0x22, 0x22, 0x43, 0x14, 0x00 },
            { 0x00, 0x41, 0x22, 0x22, 0x22, 0x32, 0x14, 0x00 },
            { 0x00, 0x10, 0x27, 0x22, 0x77, 0x32, 0x14, 0x00 },
            { 0x10, 0x11, 0x27, 0x22, 0x77, 0x32, 0x11, 0x00 },
            { 0x10, 0x44, 0x22, 0x22, 0x22, 0x44, 0x13, 0x00 },
            { 0x10, 0x33, 0x33, 0x33, 0x33, 0x33, 0x14, 0x00 },
            { 0x00, 0x31, 0x34, 0x33, 0x33, 0x43, 0x01, 0x00 },
            { 0x00, 0x41, 0x33, 0x33, 0x43, 0x66, 0x01, 0x00 },
            { 0x00, 0x10, 0x24, 0x33, 0x54, 0x55, 0x01, 0x00 },
            { 0x00, 0x10, 0x45, 0x44, 0x55, 0x55, 0x01, 0x00 },
            { 0x00, 0x10, 0x66, 0x11, 0x11, 0x66, 0x01, 0x00 },
            { 0x00, 0x00, 0x61, 0x01, 0x00, 0x61, 0x01, 0x00 },
            { 0x00, 0x00, 0x10, 0x01, 0x00, 0x10, 0x01, 0x00 }
        }
    };

    constexpr u8 k_game_world_design_width { 18 };
    constexpr u8 k_game_world_design_height{ 35 };
    constexpr u32 k_world_width { k_game_world_design_width  * k_sprite_size };
    constexpr u32 k_world_height{ k_game_world_design_height * k_sprite_size };

    constexpr u32 k_white_noise_texture_width { k_world_width  };
    constexpr u32 k_white_noise_texture_height{ k_world_height };
    constexpr u32 k_fractal_noise_texture_width { k_white_noise_texture_width  };
    constexpr u32 k_fractal_noise_texture_height{ k_white_noise_texture_height };

    // Setup the game world design.
    // This is not used directly, as it would be wasteful to store this array in the executable. The border around the
    // edges are implied, empty space is not stored and the rest of the blocks are RLE compressed. This design is also
    // used to determine the player's starting position (marked with an 's').

    constexpr char k_game_world_design[]
    {
        "bbbbbbbbbbbbbbbbbb"
        "b                b"
        "b                b"
        "b                b"
        "bs               b"
        "b                b"
        "b   bbbbbbbb     b"
        "b  bb            b"
        "b                b"
        "b  bbbbb bbb     b"
        "b                b"
        "b2               b"
        "b   bbbbbb       b"
        "b            bbbbb"
        "b   3     b      b"
        "b  3b            b"
        "b 3bb            b"
        "b      b         b"
        "b                b"
        "b2        b2     b"
        "bb2      3bbb    b"
        "bbb2    3b       b"
        "bbbb2  3b4       b"
        "b            bb  b"
        "b                b"
        "b      3b       fb"
        "b    b           b"
        "b                b"
        "b    b     b     b"
        "b   fb     b     b"
        "bbbbbb           b"
        "b          bbb bbb"
        "b                b"
        "bf    3bbb2      b"
        "bbbbbbbbbbbbbbbbbb"
    };
    static_assert(
        sizeof(k_game_world_design) == static_cast<usize>(k_game_world_design_width) * k_game_world_design_height + 1u
    );

    constexpr vec2<fixed16_16> k_player_start_location = []()
    {
        for (u16 y{ 0 }; y < k_game_world_design_height; ++y)
        {
            for (u16 x{ 0 }; x < k_game_world_design_width; ++x)
            {
                if (k_game_world_design[(y * k_game_world_design_width) + x] == 's')
                {
                    return vec2<fixed16_16>{ 
                        fixed16_16{ static_cast<i16>(x * k_sprite_size) },
                        fixed16_16{ static_cast<i16>(y * k_sprite_size) }
                    };
                }
            }
        }
    }();

    // Perform RLE compression of the game world design.
    // This runs at compile time and compresses the design above by about 90%. It works by finding horizontal runs of
    // the same type and grouping multiple runs of the same type into an array. In the end we have one array per type
    // over which we can iterate quite simply and execute the appropriate callback.

    struct sprite_run
    {
        u8 x, y;
        u8 length;
    };

    consteval u16 calculate_runs(char ch, sprite_run* buf)
    {
        u16 count{ 0 };

        // Go through each row.
        for (u8 y{ 1 }; y < k_game_world_design_height - 1u; ++y)
        {
            u8 x{ 1 };

            // Scan the columns for runs of the same kind.
            while (x < k_game_world_design_width - 1u)
            {
                // Find the index of the first (if any) block of the requested kind.
                for (; x < k_game_world_design_width - 1u; ++x)
                {
                    if (k_game_world_design[y * k_game_world_design_width + x] == ch)
                    {
                        break;
                    }
                }

                // Check if one was found.
                if (x < k_game_world_design_width - 1u)
                {
                    u8 const start_x{ x };

                    // Calculate the length of the run (max length of 256 per run).
                    u16 length{ 1 };
                    for (x = x + 1u; x < k_game_world_design_width - 1u && length < 256u; ++x, ++length)
                    {
                        if (k_game_world_design[y * k_game_world_design_width + x] != ch)
                        {
                            break;
                        }
                    } 

                    // Check if a buffer is allocated for the data.
                    if (buf != nullptr)
                    {
                        buf[count].x      = start_x;
                        buf[count].y      = y;
                        buf[count].length = static_cast<u8>(length - 1u); // A length of 0 makes no sense, so the true
                                                                          // length is always +1.
                    }
                
                    ++count;
                }
            }
        }

        return count;
    }

    constexpr auto k_game_world_design_compressed = []() consteval
    {
        struct
        {
            sprite_run buf1[calculate_runs('b', nullptr)];
            sprite_run buf2[calculate_runs('2', nullptr)];
            sprite_run buf3[calculate_runs('3', nullptr)];
            sprite_run buf4[calculate_runs('4', nullptr)];
        } result{};

        calculate_runs('b', result.buf1);
        calculate_runs('2', result.buf2);
        calculate_runs('3', result.buf3);
        calculate_runs('4', result.buf4);

        return result;
    }();

    // Setup the player struct.
    // To simplify some calculations, the player's origin is considered to be in the top-left corner.

    struct player
    {
        static constexpr u16 k_width { 13 };
        static constexpr u16 k_height{ 17 };

        vec2<fixed16_16> pos;
        vec2<fixed16_16> vel;

        bool flying;
        bool sliding;
        bool charging;
        u8   facing; // 0 = right, 1 = left
    };

    // Setup the camera struct.
    // To simplify some calculations, the camera's origin is considered to be in the top-left corner.

    struct camera
    {
        static constexpr u16 k_width { k_sprite_size * 14 };
        static constexpr u16 k_height{ k_sprite_size *  8 };

        u16 x, y;
    };

    // Setup some global state.

    constinit player g_player{ 
        .pos = k_player_start_location,
        .vel = vec2<fixed16_16>{ fixed16_16{ 0 }, fixed16_16{ 0 } },
        .flying = true
    };

    camera    g_camera;

    HWND      g_hWnd;
    HDC       g_hDC;
    POINTS    g_cursor;

    constinit vec2<u16> g_client_area{ vec2<u16>{ camera::k_width * 2, camera::k_height * 2 } };
    vec4<u16>           g_viewport;

    struct
    {
        bool W     : 1;
        bool A     : 1;
        bool S     : 1;
        bool D     : 1;
        bool Space : 1;
        bool LMB   : 1; // Unlike the other inputs, this is true on RELEASE, and gets cleared after being processed.
    } g_input;

    GLuint g_particle_buffer_id;
    u32    g_particle_count;
    bool   g_particle_init;

    GLuint g_vao;
    GLuint g_compute_particle_emitter_program_id;
    GLuint g_compute_particle_updater_program_id;
    GLuint g_render_program_id;
    GLuint g_sprite_render_program_id;
    GLuint g_background_renderer_program_id;
    GLuint g_upscaler_program_id;
    GLuint g_sprites_vertex_buffer_id;
    GLuint g_sprites_index_buffer_id;
    GLuint g_sprites_texture_array_id;
    GLuint g_index_buffer_id;
    GLuint g_atomic_counter_buffer_id;
    GLuint g_gradient_map_texture_id;
    sprite_vertex   g_sprites_vertex_buffer_storage[k_sprites_max_vertex_count];
    constinit auto* g_sprites_vertex_buffer_storage_ptr{ g_sprites_vertex_buffer_storage };
    GLuint g_framebuffer_texture_id;
    GLuint g_framebuffer_id;
    GLuint g_background_texture_id;
    GLuint g_active_particles;

    bool g_game_world_collision_map[k_world_height][k_world_width];
    fixed16_16 g_game_world_distance_field[k_world_height][k_world_width];

    alignas(u32) u8 g_white_noise_texture[k_white_noise_texture_height][k_white_noise_texture_width];
    u8 g_fractal_noise_texture[k_fractal_noise_texture_height][k_fractal_noise_texture_width];

    // Setup the collision map.
    // This is a per-pixel collision bitmap of the world. A value of 0/false in this bitmap indicates that the pixel is
    // not covered by a collidable tile. A value of 1/true on the other hand, indicates that the pixel is covered by a
    // collidable tile. I have chosen 0/false as the indicator that the pixel is not covered, as this map is
    // automatically initialized to 0/false for us when the program loads. This way we only need to fill in 1/true
    // where needed. This step uses the RLE compressed game world design to invoke specialized callbacks to draw
    // different shapes into the bitmap.

    __declspec(noinline) void __fastcall draw_variable_rectangle_sprite(u16 x, u16 y, u16 w, u16 h)
    {
        bool(*row)[k_world_width] { &g_game_world_collision_map[y] };

        for (u16 i{ 0 }; i < h; ++i, ++row)
        {
            __stosb(reinterpret_cast<u8*>(&(*row)[x]), 1, w);
        }
    }

    // Draws a full square ⬛
    void draw_full_square_sprite(u16 x, u16 y)
    {
        draw_variable_rectangle_sprite(x, y, k_sprite_size, k_sprite_size);
    }

#if 0
    // Draws the upper half of a square ⬒
    void draw_upper_half_square_sprite(u16 x, u16 y)
    {
        draw_variable_rectangle_sprite(x, y, k_sprite_size, k_sprite_size / 2);
    }

    // Draws the lower half of a square ⬓
    void draw_lower_half_square_sprite(u16 x, u16 y)
    {
        draw_variable_rectangle_sprite(x, y + k_sprite_size / 2, k_sprite_size, k_sprite_size / 2);
    }

    // Draws a horizontal bar ▬ with the height of half a sprite
    void draw_horizontal_bar_sprite(u16 x, u16 y)
    {
        draw_variable_rectangle_sprite(x, y + k_sprite_size / 2, k_sprite_size, k_sprite_size / 2);
    }
#endif

    // Draws a lower left triangle ⬕
    void draw_lower_left_triangle_sprite(u16 x, u16 y)
    {
        bool(*row)[k_world_width] { &g_game_world_collision_map[y] };

        for (u16 i{ 0 }; i < k_sprite_size; ++i, ++row)
        {
            __stosb(reinterpret_cast<u8*>(&(*row)[x]), 1, i + 1);
        }
    }

    // Draws a lower right triangle ◪
    void draw_lower_right_triangle_sprite(u16 x, u16 y)
    {
        bool(*row)[k_world_width] { &g_game_world_collision_map[y] };

        for (u16 i{ 0 }; i < k_sprite_size; ++i, ++row)
        {
            __stosb(reinterpret_cast<u8*>(&(*row)[(x + k_sprite_size) - (i + 1)]), 1, i + 1);
        }
    }

    // Draws a upper left triangle ◩
    void draw_upper_left_triangle_sprite(u16 x, u16 y)
    {
        bool(*row)[k_world_width] { &g_game_world_collision_map[y] };

        for (u16 i{ 0 }; i < k_sprite_size; ++i, ++row)
        {
            __stosb(reinterpret_cast<u8*>(&(*row)[x]), 1, (k_sprite_size) - i);
        }
    }

    void compute_game_world_collision_map()
    {
        // Draw the top and bottom borders
        for (u8 x{ 0 }; x < k_game_world_design_width; ++x)
        {
            draw_full_square_sprite(x * k_sprite_size, 0);
            draw_full_square_sprite(x * k_sprite_size, (k_game_world_design_height - 1) * k_sprite_size);
        }
        // Draw the left and right borders
        for (u8 y{ 0 }; y < k_game_world_design_height; ++y)
        {
            draw_full_square_sprite(0, y * k_sprite_size);
            draw_full_square_sprite((k_game_world_design_width - 1) * k_sprite_size, y * k_sprite_size);
        }

        constexpr array_view<sprite_run> runs[]
        {
            k_game_world_design_compressed.buf1,
            k_game_world_design_compressed.buf2,
            k_game_world_design_compressed.buf3,
            k_game_world_design_compressed.buf4,
        };

        constexpr void(*(draw_fn[countof(runs)]))(u16 x, u16 y) {
            draw_full_square_sprite,
            draw_lower_left_triangle_sprite,
            draw_lower_right_triangle_sprite,
            draw_upper_left_triangle_sprite,
        };

        // Draw the world
        for (u8 i{ 0 }; i < countof(runs); ++i)
        {
            auto const fn = draw_fn[i];
            for (sprite_run run : runs[i])
            {
                u8 const start_x{ run.x };
                u8 const start_y{ run.y };

                for (u8 j{ 0 }; j < (run.length + 1u); ++j)
                {
                    fn((start_x + j) * k_sprite_size, start_y * k_sprite_size);
                }
            }
        }
    }

    // Setup the player collision map.
    // This is a per-pixel collision bitmap of the world from the player's point of view. A value of 0/false indicates
    // that the player's origin (top-left corner) can be safely located there without the player's collision box
    // intersecting any collidable tiles. A value of 1/true on the other hand, indicates that the player's collision
    // box would intersect a collidable tile if the player's origin would be located there.
    // This map is essentially a bitmap representation of the minkowski sum of all collidable tiles and the player's
    // collision box. We perform this minkowski sum precomputation so that we can get pixel-perfect collision detection
    // without any tunneling or intersection problems by tracing a line through this map from where the origin is, to
    // where the origin wants to be on the next frame. Implementing this line tracing is cheap.

    constexpr u32 k_player_collision_map_width { k_world_width  - (player::k_width  - 1) };
    constexpr u32 k_player_collision_map_height{ k_world_height - (player::k_height - 1) };
    bool g_player_collision_map[k_player_collision_map_height][k_player_collision_map_width];

    void compute_player_collision_map()
    {
        for (u32 y{ 0 }; y < k_player_collision_map_height; ++y)
        {
            for (u32 x{ 0 }; x < k_player_collision_map_width; ++x)
            {
                [x, y]()
                {
                    for (u32 i{ 0 }; i < player::k_height; ++i)
                    {
                        for (u32 j{ 0 }; j < player::k_width; ++j)
                        {
                            if (g_game_world_collision_map[y + i][x + j])
                            {
                                g_player_collision_map[y][x] = 1;
                                return;
                            }
                        }
                    }
                }();
            }
        }
    }

    // Setup the window and input handling.

    void adjust_viewport()
    {
        // The viewport gets adjusted such that the game can be upscaled by an integer multiplier without extending
        // past the available client area. Black bars get added around the edges as necessary to make sure it is
        // centered.

        u16 const tmp_0{ static_cast<u16>(g_client_area.x / camera::k_width ) };
		u16 const tmp_1{ static_cast<u16>(g_client_area.y / camera::k_height) };

		if ((tmp_1 * camera::k_width) <= g_client_area.x)
		{
            u16 const tmp_2{ static_cast<u16>(tmp_1 * camera::k_width ) };
            u16 const tmp_3{ static_cast<u16>(tmp_1 * camera::k_height) };

            u16 const offset_x{ static_cast<u16>((g_client_area.x - tmp_2) / 2Ui16) };
            u16 const offset_y{ static_cast<u16>((g_client_area.y - tmp_3) / 2Ui16) };

            g_viewport = vec4<u16>{ offset_x, offset_y, tmp_2, tmp_3 };
		}
		else
		{
            u16 const tmp_2{ static_cast<u16>(tmp_0 * camera::k_width ) };
            u16 const tmp_3{ static_cast<u16>(tmp_0 * camera::k_height) };

            u16 const offset_x{ static_cast<u16>((g_client_area.x - tmp_2) / 2Ui16) };
            u16 const offset_y{ static_cast<u16>((g_client_area.y - tmp_3) / 2Ui16) };

            g_viewport = vec4<u16>{ offset_x, offset_y, tmp_2, tmp_3 };
		}
    }

    void handle_key(u8 virtual_key, bool key_down)
    {
        switch (virtual_key)
        {
            case 'W':
                g_input.W = key_down;
                break;

            case 'A':
                g_input.A = key_down;
                break;

            case 'S':
                g_input.S = key_down;
                break;

            case 'D':
                g_input.D = key_down;
                break;

            case VK_SPACE:
                g_input.Space = key_down;
                break;

            case VK_ESCAPE:
                // Imagine being nice and cleaning up our resources lmao.
                ExitProcess(0);

            // Ignore everything else.
            default:
                break;
        }
    }

    LRESULT WINAPI window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
            case WM_CLOSE:
                // Imagine being nice and cleaning up our resources lmao.
                ExitProcess(0);

            case WM_PAINT:
            {
                // This paint occurs once after window creation and before the first draw with OpenGL.
                // It simply clears the screen and writes the text "Loading. . .".

                PAINTSTRUCT ps;
                BeginPaint(hWnd, &ps);
                BitBlt(
                    g_hDC,
                    ps.rcPaint.left,
                    ps.rcPaint.top,
                    (ps.rcPaint.right  - ps.rcPaint.left),
                    (ps.rcPaint.bottom - ps.rcPaint.top),
                    nullptr,
                    0, 0,
                    WHITENESS
                );
                DrawTextA(g_hDC, "Loading. . .", 12, &ps.rcPaint, DT_CENTER | DT_VCENTER  | DT_SINGLELINE | DT_NOCLIP);
                EndPaint(hWnd, &ps);
            } break;

            case WM_KEYDOWN:
                // Check for Alt + Enter shortcut for toggling fullscreen.
                if ((HIWORD(lParam) & KF_ALTDOWN) && (wParam == VK_RETURN))
                {
                    goto toggle_fullscreen;
                }

                // Check if it is any other key we care about and set the state to down.
                handle_key(static_cast<u8>(wParam), true);
                break;

            case WM_KEYUP:
                // Check if it is a key we care about and set the state to down.
                handle_key(static_cast<u8>(wParam), false);
                break;

            case WM_SYSKEYDOWN:
                // Check for Right Alt + Enter shortcut for toggling fullscreen.
                if ((HIWORD(lParam) & KF_ALTDOWN) && (wParam == VK_RETURN))
                {
                    goto toggle_fullscreen;
                }

                // We do not care, let the default handler deal with it.
                goto default_case;

            case WM_MOUSEMOVE:
                // Update the cursor position.
                g_cursor = MAKEPOINTS(lParam);
                break;

            case WM_LBUTTONUP:
                // Left mouse button released.
                g_input.LMB = true;
                break;

            case WM_SYSCOMMAND:
                // Check for maximize / restore command.
                if ((wParam & 0xFFF0) == SC_MAXIMIZE)
                {
                toggle_fullscreen:
                    static bool      is_fullscreen{ false };
                    static RECT      old_window_rect;
                    static vec2<u16> old_client_area;

                    // Check if currently windowed.
                    if (!is_fullscreen)
                    {
                        // Save the old position and size.
                        GetWindowRect(g_hWnd, &old_window_rect);
                        old_client_area = g_client_area;

                        // Get the monitor the window is in.
                        HMONITOR const monitor = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
                        if (monitor != nullptr)
                        {
                            // Get the monitor size.
                            MONITORINFO monitor_info; monitor_info.cbSize = sizeof(MONITORINFO);
                            if (GetMonitorInfoA(monitor, &monitor_info))
                            {
                                // Make the window fullscreen.
                                SetWindowLongA(g_hWnd, GWL_STYLE, WS_OVERLAPPED | WS_VISIBLE);
                                SetWindowPos(
                                    g_hWnd,
                                    HWND_TOP,
                                    monitor_info.rcMonitor.left,
                                    monitor_info.rcMonitor.top,
                                    monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left,
                                    monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                                    SWP_FRAMECHANGED | SWP_SHOWWINDOW
                                );

                                // Save the size of the screen, so we know how much to scale up the image.
                                g_client_area = vec2<u16>{
                                    static_cast<u16>(monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left),
                                    static_cast<u16>(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top)
                                };

                                // Adjust the viewport for rendering.
                                adjust_viewport();

                                // Mark as fullscreen.
                                is_fullscreen = true;
                            }
                        }
                    }
                    else
                    {
                        // Restore the previous state.
                        SetWindowLongA(g_hWnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME) | WS_VISIBLE);
                        SetWindowPos(
                            g_hWnd,
                            HWND_TOP,
                            old_window_rect.left,
                            old_window_rect.top,
                            old_window_rect.right  - old_window_rect.left,
                            old_window_rect.bottom - old_window_rect.top,
                            SWP_FRAMECHANGED | SWP_SHOWWINDOW
                        );
                        g_client_area = old_client_area;
                        g_viewport    = vec4<u16>{ 0, 0, g_client_area.x, g_client_area.y };

                        // Mark as no longer fullscreen.
                        is_fullscreen = false;
                    }
                    break;
                }

                [[fallthrough]];
            default: default_case:
                return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    void init_window()
    {
        G21_DEBUG_PRINT("#DEBUG: Initializing window.\n");

        static WNDCLASSEX wc{};
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.hInstance     = nullptr; // GetModuleHandle(nullptr);
        wc.style         = CS_OWNDC;
        wc.lpfnWndProc   = window_proc;
        wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
        wc.lpszClassName = "4MBGameJam2021";
        RegisterClassExA(&wc);

        g_viewport = vec4<u16>{ 0, 0, g_client_area.x, g_client_area.y };

        RECT r;
        r.left   = (GetSystemMetrics(SM_CXSCREEN) - static_cast<i32>(static_cast<u32>(g_client_area.x))) / 2;
        r.top    = (GetSystemMetrics(SM_CYSCREEN) - static_cast<i32>(static_cast<u32>(g_client_area.y))) / 2;
        r.right  = r.left + static_cast<i32>(static_cast<u32>(g_client_area.x));
        r.bottom = r.top  + static_cast<i32>(static_cast<u32>(g_client_area.y));
        AdjustWindowRect(&r, (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME), false);

        g_hWnd = CreateWindowExA
        (
            0,
            wc.lpszClassName,
            wc.lpszClassName,
            (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME) | WS_VISIBLE,
            r.left, r.top,
            (r.right - r.left), (r.bottom - r.top), 
            nullptr, nullptr,
            wc.hInstance,
            nullptr
        );

        g_hDC = GetDC(g_hWnd);
    }

    // Setup OpenGL.

    #define GLFUNCS \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
    X(PFNGLCLEARBUFFERDATAPROC, glClearBufferData) \
    X(PFNGLCLEARBUFFERUIVPROC, glClearBufferuiv) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLINVALIDATEBUFFERDATAPROC, glInvalidateBufferData) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLMAPBUFFERPROC, glMapBuffer) \
    X(PFNGLMEMORYBARRIERPROC, glMemoryBarrier) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLTEXIMAGE3DPROC, glTexImage3D) \
    X(PFNGLTEXSUBIMAGE3DPROC, glTexSubImage3D) \
    X(PFNGLTEXSTORAGE3DPROC, glTexStorage3D) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2IPROC, glUniform2i) \
    X(PFNGLUNIFORM4IPROC, glUniform4i) \
    X(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer) \
    X(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT)

    PROC _gl_fnptrs[36];

    #define glActiveTexture ((PFNGLACTIVETEXTUREPROC)_gl_fnptrs[0])
    #define glAttachShader ((PFNGLATTACHSHADERPROC)_gl_fnptrs[1])
    #define glBindBuffer ((PFNGLBINDBUFFERPROC)_gl_fnptrs[2])
    #define glBindBufferBase ((PFNGLBINDBUFFERBASEPROC)_gl_fnptrs[3])
    #define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)_gl_fnptrs[4])
    #define glBindVertexArray ((PFNGLBINDVERTEXARRAYPROC)_gl_fnptrs[5])
    #define glBufferData ((PFNGLBUFFERDATAPROC)_gl_fnptrs[6])
    #define glBufferSubData ((PFNGLBUFFERSUBDATAPROC)_gl_fnptrs[7])
    #define glClearBufferData ((PFNGLCLEARBUFFERDATAPROC)_gl_fnptrs[8])
    #define glClearBufferuiv ((PFNGLCLEARBUFFERUIVPROC)_gl_fnptrs[9])
    #define glCreateProgram ((PFNGLCREATEPROGRAMPROC)_gl_fnptrs[10])
    #define glCreateShader ((PFNGLCREATESHADERPROC)_gl_fnptrs[11])
    #define glCompileShader ((PFNGLCOMPILESHADERPROC)_gl_fnptrs[12])
    #define glDispatchCompute ((PFNGLDISPATCHCOMPUTEPROC)_gl_fnptrs[13])
    #define glEnableVertexAttribArray ((PFNGLENABLEVERTEXATTRIBARRAYPROC)_gl_fnptrs[14])
    #define glFramebufferTexture ((PFNGLFRAMEBUFFERTEXTUREPROC)_gl_fnptrs[15])
    #define glGenBuffers ((PFNGLGENBUFFERSPROC)_gl_fnptrs[16])
    #define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)_gl_fnptrs[17])
    #define glGenVertexArrays ((PFNGLGENVERTEXARRAYSPROC)_gl_fnptrs[18])
    #define glGetBufferSubData ((PFNGLGETBUFFERSUBDATAPROC)_gl_fnptrs[19])
    #define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)_gl_fnptrs[20])
    #define glInvalidateBufferData ((PFNGLINVALIDATEBUFFERDATAPROC)_gl_fnptrs[21])
    #define glLinkProgram ((PFNGLLINKPROGRAMPROC)_gl_fnptrs[22])
    #define glMapBuffer ((PFNGLMAPBUFFERPROC)_gl_fnptrs[23])
    #define glMemoryBarrier ((PFNGLMEMORYBARRIERPROC)_gl_fnptrs[24])
    #define glShaderSource ((PFNGLSHADERSOURCEPROC)_gl_fnptrs[25])
    #define glTexImage3D ((PFNGLTEXIMAGE3DPROC)_gl_fnptrs[26])
    #define glTexSubImage3D ((PFNGLTEXSUBIMAGE3DPROC)_gl_fnptrs[27])
    #define glTexStorage3D ((PFNGLTEXSTORAGE3DPROC)_gl_fnptrs[28])
    #define glUniform1i ((PFNGLUNIFORM1IPROC)_gl_fnptrs[29])
    #define glUniform2i ((PFNGLUNIFORM2IPROC)_gl_fnptrs[30])
    #define glUniform4i ((PFNGLUNIFORM4IPROC)_gl_fnptrs[31])
    #define glUnmapBuffer ((PFNGLUNMAPBUFFERPROC)_gl_fnptrs[32])
    #define glUseProgram ((PFNGLUSEPROGRAMPROC)_gl_fnptrs[33])
    #define glVertexAttribIPointer ((PFNGLVERTEXATTRIBIPOINTERPROC)_gl_fnptrs[34])
    #define wglSwapIntervalEXT ((PFNWGLSWAPINTERVALEXTPROC)_gl_fnptrs[35])

    #ifdef _DEBUG
    PFNGLGETSHADERIVPROC      glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

    void APIENTRY gl_debug_callback(GLenum, GLenum, GLuint, GLenum, GLsizei length, GLchar const* message, void const*)
    {
        WriteConsoleA(_g21_debug_out, message, length, nullptr, nullptr);
        WriteConsoleA(_g21_debug_out, "\n", 1, nullptr, nullptr);
    }
    #endif

    __forceinline void load_gl_functions()
    {
        G21_DEBUG_PRINT("#DEBUG: Loading OpenGL functions.\n");

        #ifdef _DEBUG
            // Unroll all the calls to wglGetProcAddress to simplify generating the error messages.
            i32 i{ 0 };
            #define X(t, n)                                               \
            if ((_gl_fnptrs[i++] = wglGetProcAddress(#n)) == nullptr)     \
            {                                                             \
                G21_DEBUG_PRINT("#DEBUG: " #n " could not be loaded.\n"); \
            }
            GLFUNCS
            #undef X

            glGetShaderiv      = reinterpret_cast<PFNGLGETSHADERIVPROC>     (wglGetProcAddress("glGetShaderiv"));
            glGetShaderInfoLog = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(wglGetProcAddress("glGetShaderInfoLog"));
        #else
            constexpr u32 gl_function_count{ sizeof(_gl_fnptrs) / sizeof(PROC) };

            // Prepare a pointer to a list of required OpenGL function names.
            char const* p
            {
                #define X(t, n) #n "\0"
                GLFUNCS
                #undef X
            };

            for (i32 i{ 0 }; i < gl_function_count; ++i, ++p)
            {
                // Get the address of the function implementation within the driver
                _gl_fnptrs[i] = wglGetProcAddress(p);

                // Advance to the next function name
                for (; *p != '\0'; ++p);
            }
        #endif
    }

    #undef GLFUNCS

    // TODO: This could be compressed

    constexpr char k_fullscreen_quad_vs_source[]
    {
        "#version 430 core\n"

        "out vec2 uv;"

        "void main()"
        "{"
            "float x=-1+float((gl_VertexID&1)<<2);"
            "float y=-1+float((gl_VertexID&2)<<1);"
            "uv.x=(x+1)*0.5;"
            "uv.y=(y+1)*0.5;"
            "gl_Position=vec4(x,y,0,1);"
        "}"
    };

    constexpr char k_background_render_fs_source[]
    {
        "#version 430 core\n"

        "layout(location = 0) uniform ivec4 camera;"

        "layout(binding = 0) uniform usampler2DRect tex;"

        "in vec2 uv;"

        "void main(){"
            "gl_FragColor=vec4(texelFetch(tex, camera.xy + ivec2(vec2(uv.x,1-uv.y)*camera.zw)).rgb/255.,1);"
        "}"
    };

    constexpr char k_texture_blit_fs_source[]
    {
        "#version 430 core\n"

        "layout(binding = 0) uniform sampler2D tex;"

        "in vec2 uv;"

        "void main(){"
            "gl_FragColor=vec4(texture(tex, uv).rgb,1);"
        "}"
    };

    constexpr char k_sprite_render_vs_source[]
    {
        "#version 430 core\n"

        "layout(location = 0) in ivec3 vertexPosition;"

        "layout(location = 0) uniform ivec4 camera;"

        "out vec2 uv;"
        "flat out uint index;"

        "void main(){"
            "uv = vec2(float((gl_VertexID & 2) >> 1), float(gl_VertexID & 1));"
            "index = vertexPosition.z;"
            "ivec2 p = vertexPosition.xy >> 16;"
            "gl_Position=vec4((2.0 * vec2(p - camera.xy) / camera.zw) - 1.0, 0, 1);"
            "gl_Position.y *= -1.0;"
        "}"
    };

    constexpr char k_sprite_render_fs_source[]
    {
        "#version 430 core\n"

        "layout(binding = 0) uniform usampler2DArray tex;"

        "in vec2 uv;"
        "flat in uint index;"

        "void main(){"
            "gl_FragColor=vec4(texture(tex, vec3(uv, index)))/255.0;"
        "}"
    };
    
#if 0
    constexpr char k_particle_render_vs_source[]
    {
        "#version 430 core\n"

        "layout(location = 0) in ivec2 vertexPosition;"

        "layout(location = 0) uniform ivec4 camera;"

        "out vec2 c;"

        "void main(){"
#if 0
            "float x=-1+float((gl_VertexID&1)<<2);"
            "float y=-1+float((gl_VertexID&2)<<1);"
            "gl_Position=vec4(x,y,0,1);"
#else
            "vec2 p = vertexPosition/65536.;"
            "c=vec2((cos(gl_VertexID) + 1.0)/2.0,(sin(gl_VertexID) + 1.0)/2.0);"

            "gl_Position=vec4((2.0 * vec2(p - camera.xy) / camera.zw) - 1.0, 0, 1);"
            "gl_Position.y *= -1.0;"
#endif
        "}"
    };

    constexpr char k_particle_render_fs_source[]
    {
        "#version 430 core\n"

        "layout(location = 0) uniform ivec4 camera;"
        "layout(location = 1) uniform ivec2 res;"

        "layout(binding = 0) uniform isampler2DRect tex;"

        "in vec2 c;"

        "void main(){"
#if 0
            "int x = int(camera.x + (gl_FragCoord.x/float(res.x))*camera.z);"
            "int y = int(camera.y + ((float(res.y) - gl_FragCoord.y)/float(res.y))*camera.w);"
            "vec2 value = vec2(texelFetch(tex, ivec2(x, y)).xy / 65536.) / 40.;"
            "gl_FragColor=vec4(0.5 + value.x/2., 0.5 + value.y/2., 0, 1);"
#else
            "gl_FragColor=vec4(0.2, 0.2, 0.2, 0.0495);"
#endif
        "}"
    };

#   define PARTICLE_DRAG 0.94
#   define LOCAL_SIZE_X 32
#   define PARTICLE_STRUCT                                  \
        "struct particle{"                                  \
            "ivec2 cur_pos;"                                \
            "ivec2 old_pos;" /*TODO: switch to velocity?*/  \
            "uint life;"                                    \
          /*"uint _pad;"*/                                  \
        "};"

    // This merely mimics the definition above.
    struct cs_particle
    {
        alignas(8) vec2<fixed16_16> cur_pos;
        alignas(8) vec2<fixed16_16> old_pos;
        u32                         life;
        u32 : 1; // Padding.
    };
    static_assert(sizeof(cs_particle) == 24);

    constexpr char k_particle_emit_cs_source[]
    {
        "#version 430\n"
        "#extension GL_ARB_compute_shader : enable\n"
        "#extension GL_ARB_shader_storage_buffer_object : enable\n"

        PARTICLE_STRUCT

        "layout(std430,binding=0) buffer _0{"
            "particle particles[];"
        "};"

        "layout(location = 0) uniform int particle_count;"
        "layout(location = 1) uniform ivec2 emit_location;"

        "layout (local_size_x = " G21_STRINGIFY(LOCAL_SIZE_X) ", local_size_y = 1, local_size_z = 1) in; "
        "void main(){"
            "uint gid = gl_GlobalInvocationID.x;"

            "if (gid < particle_count){"
                "particle p;"

                "p.old_pos = emit_location;"
                "float f = float(gid) / float(particle_count);"
                "float c = cos(f * 6.28318)*sin(gid) + float(1 - int((gid & 1) << 1))/2.0;"
                "float s = sin(f * 6.28318)*sin(gid) + float(1 - int(gid & 2))/2.0;"
                "p.cur_pos = p.old_pos + ivec2(int(c * 200000.0), int(s * 200000.0));"
                "p.life = gid/20 + 50;"

                "particles[gid] = p;"
            "}"
        "}"
    };

    constexpr char k_particle_update_cs_source[]
    {
        "#version 430\n"
        "#extension GL_ARB_compute_shader : enable\n"
        "#extension GL_ARB_shader_storage_buffer_object : enable\n"

        PARTICLE_STRUCT

        "layout(std430,binding=0) buffer _0{"
            "particle particles[];"
        "};"
        "layout(std430,binding=1) writeonly buffer _1{"
            "uint indices[];"
        "};"
        "layout(std430,binding=2) buffer _2{"
            "uint alive_counter;"
        "};"

        "layout(location = 0) uniform int particle_count;"
        "layout(binding = 0) uniform isampler2DRect tex;"

        "layout (local_size_x=" G21_STRINGIFY(LOCAL_SIZE_X) ", local_size_y=1, local_size_z=1) in; "
        "void main(){"
            "uint gid = gl_GlobalInvocationID.x;"

            "if (gid < particle_count){"
                "particle p = particles[gid];"

                "if (p.life != 0){"
                    "indices[atomicAdd(alive_counter,1)] = gid;"
                    "p.life--;"
                        
                    "ivec2 b = texelFetch(tex, p.cur_pos >> 16).xy >> 8;"

                    "ivec2 d = p.cur_pos - p.old_pos;"
                    "p.old_pos = p.cur_pos;"
                    "p.cur_pos += ivec2(((vec2(d)/32768.) * " G21_STRINGIFY(PARTICLE_DRAG) ") * 32768.) + b * 2;"

                    "particles[gid] = p;"
                "}"
            "}"
        "}"
    };
#endif

    GLuint compile_shader(GLenum shader_type, GLchar const* source)
    {
        // Create and compile the shader.
        GLuint const shader_id{ glCreateShader(shader_type) };
        glShaderSource (shader_id, 1, &source, nullptr);
        glCompileShader(shader_id);

        #ifdef _DEBUG
        // Check for problems.
        {
            GLint result{ GL_FALSE }, log_length{ 0 };
            glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
            glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
            if (log_length > 0 && !result)
            {
                static GLchar buffer[1024];
                if (log_length > sizeof(buffer))
                {
                    log_length = sizeof(buffer);
                }
                glGetShaderInfoLog(shader_id, log_length, nullptr, buffer);
                WriteConsoleA(_g21_debug_out, buffer, log_length, nullptr, nullptr);
                __fastfail(FAST_FAIL_FATAL_APP_EXIT);
            }
        }
        #endif

        // Return the id.
        return shader_id;
    }

    __forceinline void load_shaders()
    {
        G21_DEBUG_PRINT("#DEBUG: Loading shaders.\n");

        /*
        // Load the compute shader for the particle emitter.
        g_compute_particle_emitter_program_id = glCreateProgram();
        glAttachShader(
            g_compute_particle_emitter_program_id,
            compile_shader(GL_COMPUTE_SHADER, k_particle_emit_cs_source)
        );
        glLinkProgram(g_compute_particle_emitter_program_id);

        // Load the compute shader for the particle updater.
        g_compute_particle_updater_program_id = glCreateProgram();
        glAttachShader(
            g_compute_particle_updater_program_id,
            compile_shader(GL_COMPUTE_SHADER, k_particle_update_cs_source)
        );
        glLinkProgram(g_compute_particle_updater_program_id);

        // Load the vertex and fragment shaders for particle rendering.
        g_render_program_id = glCreateProgram();
        glAttachShader(
            g_render_program_id,
            compile_shader(GL_VERTEX_SHADER,   k_particle_render_vs_source)
        );
        glAttachShader(
            g_render_program_id,
            compile_shader(GL_FRAGMENT_SHADER, k_particle_render_fs_source)
        );
        glLinkProgram(g_render_program_id);
        */

        // Load the vertex and fragment shaders for background rendering.
        g_background_renderer_program_id = glCreateProgram();
        glAttachShader(
            g_background_renderer_program_id,
            compile_shader(GL_VERTEX_SHADER, k_fullscreen_quad_vs_source)
        );
        glAttachShader(
            g_background_renderer_program_id,
            compile_shader(GL_FRAGMENT_SHADER, k_background_render_fs_source)
        );
        glLinkProgram(g_background_renderer_program_id);

        // Load the vertex and fragment shaders for texture blitting.
        g_upscaler_program_id = glCreateProgram();
        glAttachShader(
            g_upscaler_program_id,
            compile_shader(GL_VERTEX_SHADER, k_fullscreen_quad_vs_source)
        );
        glAttachShader(
            g_upscaler_program_id,
            compile_shader(GL_FRAGMENT_SHADER, k_texture_blit_fs_source)
        );
        glLinkProgram(g_upscaler_program_id);

        // Load the vertex and fragment shaders for sprite rendering.
        g_sprite_render_program_id = glCreateProgram();
        glAttachShader(
            g_sprite_render_program_id,
            compile_shader(GL_VERTEX_SHADER,   k_sprite_render_vs_source)
        );
        glAttachShader(
            g_sprite_render_program_id,
            compile_shader(GL_FRAGMENT_SHADER, k_sprite_render_fs_source)
        );
        glLinkProgram(g_sprite_render_program_id);
    }

    __forceinline void init_gl_context()
    {
        G21_DEBUG_PRINT("#DEBUG: Initializing OpenGL context.\n");

        // Describe the pixel format we want.
        static PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion   = 1;
        pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.iLayerType = PFD_MAIN_PLANE;

        // Set the pixel format.
        SetPixelFormat(g_hDC, ChoosePixelFormat(g_hDC, &pfd), &pfd);

        // Create the (maybe temporary) OpenGL context.
        HGLRC const hGLRC = wglCreateContext(g_hDC);
        wglMakeCurrent(g_hDC, hGLRC);

        #ifdef _DEBUG
        // Load wglCreateContextAttribsARB so that we can create a debug context.
        auto const wglCreateContextAttribsARB = 
            reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));

        // Delete the old context.
        wglMakeCurrent(g_hDC, 0);
        wglDeleteContext(hGLRC);

        // Prepare the attributes for our debug context.
        constexpr int attribs[]
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            0
        };

        // Create the debug OpenGL context.
        wglMakeCurrent(g_hDC, wglCreateContextAttribsARB(g_hDC, 0, attribs));
        #endif
    }

    void render_sprite_atlas() 
    {
        static alignas(u32) vec4<u8> sprite_atlas[4][16][16];

        for (u8 i{ 0 }; i < 4; ++i)
        {
            for (u8 y{ 0 }; y < 16; ++y)
            {
                for (u8 x{ 0 }; x < 8; ++x)
                {
                    u8 const idx0{ static_cast<u8>(k_player_sprite[i][y][x] & 0x0F) };
                    u8 const idx1{ static_cast<u8>(k_player_sprite[i][y][x] >> 4)   };

                    sprite_atlas[i][y][x * 2 + 0] = k_sprite_palette[idx0];
                    sprite_atlas[i][y][x * 2 + 1] = k_sprite_palette[idx1];
                }
            }
        }

        glGenTextures(1, &g_sprites_texture_array_id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_sprites_texture_array_id);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8UI, 16, 16, 4 /* sprite count */, 0, GL_RGBA_INTEGER,
            GL_UNSIGNED_BYTE, sprite_atlas);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    __forceinline void init_sprite_atlas()
    {
        render_sprite_atlas();

        glGenBuffers(1, &g_sprites_vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, g_sprites_vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_sprites_vertex_buffer_storage), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &g_sprites_index_buffer_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sprites_index_buffer_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, k_sprites_max_index_count * sizeof(GLuint), nullptr, GL_STATIC_DRAW);
            
        void* const sprites_index_buffer_map{ glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY) };

        // Since we only render squares, we can already fill out the index buffer so we never need to touch it again.

        // Alias the pointer so that we can easily write out the values.
        GLuint* p{ static_cast<GLuint*>(sprites_index_buffer_map) };
        for (usize i{ 0 }; i < k_sprites_max_quad_count; ++i)
        {
            // Tell OpenGL how four vertices combine to form two triangles.
            p[i * 6 + 0] = static_cast<GLuint>(i * 4 + 0); // Triangle one.
            p[i * 6 + 1] = static_cast<GLuint>(i * 4 + 1); //
            p[i * 6 + 2] = static_cast<GLuint>(i * 4 + 2); //
            p[i * 6 + 3] = static_cast<GLuint>(i * 4 + 1); // Triangle two.
            p[i * 6 + 4] = static_cast<GLuint>(i * 4 + 3); //
            p[i * 6 + 5] = static_cast<GLuint>(i * 4 + 2); //
        }

        // Release the map so that OpenGL can transfer the data to the GPU.
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    __forceinline void init_framebuffer()
    {
        // Generate a texture object which will be bound to the framebuffer.
        glGenTextures(1, &g_framebuffer_texture_id);
        glBindTexture(GL_TEXTURE_2D, g_framebuffer_texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, camera::k_width, camera::k_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Generate the framebuffer object.
        glGenFramebuffers(1, &g_framebuffer_id);
        glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer_id);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_framebuffer_texture_id, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void init_gl()
    {
        G21_DEBUG_PRINT("#DEBUG: Initializing OpenGL.\n");

        // Initialize the OpenGL context.
        init_gl_context();

        // Load just the functions we need.
        load_gl_functions();

        // Enable V-Sync.
        wglSwapIntervalEXT(1);

        #ifdef _DEBUG
        // If in debug mode, activate debug output from the OpenGL driver.
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(
            wglGetProcAddress("glDebugMessageCallback")
        )(gl_debug_callback, nullptr);
        reinterpret_cast<PFNGLDEBUGMESSAGECONTROLPROC>(
            wglGetProcAddress("glDebugMessageControl")
        )(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
        #endif

        G21_DEBUG_PRINT("#DEBUG: Creating OpenGL buffers.\n");

        // Generate the framebuffer used for rendering at a fixed internal resolution.
        init_framebuffer();

#if 0
        // Generate the buffers used by the compute shader.

        glGenBuffers(1, &g_particle_buffer_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_particle_buffer_id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(cs_particle) * k_max_particle_count, nullptr, GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_particle_buffer_id);

        GLuint index_buffer_id;
        glGenBuffers(1, &g_index_buffer_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_index_buffer_id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * k_max_particle_count, nullptr, GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_index_buffer_id);

        glGenBuffers(1, &g_atomic_counter_buffer_id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_atomic_counter_buffer_id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_atomic_counter_buffer_id);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
#endif

        // Generate and bind the Vertex Array Object.
        glGenVertexArrays(1, &g_vao);
        glBindVertexArray(g_vao);

        // Enable the vertex position attribute.
        glEnableVertexAttribArray(0);

        // Initialize the sprite atlas.
        init_sprite_atlas();

#if 0
        // Bind the particle buffer SSBO as the vertex array buffer.
        glBindBuffer(GL_ARRAY_BUFFER, g_particle_buffer_id);
        glVertexAttribIPointer(0, 2, GL_INT, sizeof(cs_particle), nullptr);

        // Bind the "alive" particle index array.
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer_id);

        // Generate the texture used for particle pathfinding.
        glGenTextures(1, &g_gradient_map_texture_id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, g_gradient_map_texture_id);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB32I, k_world_width, k_world_height, 0, GL_RGB_INTEGER, GL_INT, nullptr);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
#endif

        // Load the shaders.
        load_shaders();

        // Enable blending.
        glEnable(GL_BLEND);
    }

    void push_sprite(vec2<fixed16_16> pos, vec2<u8> size, u16 sprite_texture_index)
    {
        // Top left corner.
        *(g_sprites_vertex_buffer_storage_ptr++) =
        {
            pos, sprite_texture_index
        };

        // Bottom left corner.
        *(g_sprites_vertex_buffer_storage_ptr++) =
        {
            pos + vec2<fixed16_16>{ {}, fixed16_16{ size.y } }, sprite_texture_index
        };

        // Top right corner.
        *(g_sprites_vertex_buffer_storage_ptr++) =
        {
            pos + vec2<fixed16_16>{ fixed16_16{ size.x }, {} }, sprite_texture_index
        };

        // Bottom right corner.
        *(g_sprites_vertex_buffer_storage_ptr++) =
        {
            pos + vec2<fixed16_16>{ fixed16_16{ size.x }, fixed16_16{ size.y } }, sprite_texture_index
        };
    }

    void render_sprites()
    {
        // Count sprites to render.
        auto* const sprites_vertex_buffer_begin = g_sprites_vertex_buffer_storage;
        usize const count{ static_cast<usize>(g_sprites_vertex_buffer_storage_ptr - sprites_vertex_buffer_begin) / 4U };

        glUseProgram(g_sprite_render_program_id);

        glUniform4i(0, g_camera.x, g_camera.y, camera::k_width, camera::k_height);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_sprites_texture_array_id);

        // Orphan the old vertex buffer.
        glBindBuffer(GL_ARRAY_BUFFER, g_sprites_vertex_buffer_id);
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_sprites_vertex_buffer_storage), nullptr, GL_DYNAMIC_DRAW);

        // Transfer the new vertices into a new vertex buffer.
        glBufferSubData(GL_ARRAY_BUFFER, 0, count * k_sprites_vertices_per_quad * sizeof(sprite_vertex), sprites_vertex_buffer_begin);

        // Draw the quads.
        glVertexAttribIPointer(0, 3, GL_INT, sizeof(sprite_vertex), nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sprites_index_buffer_id);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count * k_sprites_indices_per_quad), GL_UNSIGNED_INT, nullptr);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glUseProgram(0);

        // Reset pointer.
        g_sprites_vertex_buffer_storage_ptr = g_sprites_vertex_buffer_storage;
    }

    // Setup the game world distance field.
    
    void compute_game_world_distance_field(bool inverse)
    {
        static u32 sedt_x[k_world_height][k_world_width]; 
        
        // Perform the horizontal pass.
        {
            for (u32 y{ 0 }; y < k_world_height; ++y)
            {
                for (u32 x{ 0 }; x < k_world_width; ++x)
                {
                    // Check for solid block.
                    if (g_game_world_collision_map[y][x] != inverse)
                    {
                        // Store a distance of 0.
                        sedt_x[y][x] = 0;
                    }
                    else
                    {
                        // Start with an initial squared minimum distance equal to the squared width of the world.
                        u32 min{ k_world_width * k_world_width };

                        // Go through the columns.
                        for (u32 i{ 0 }; i < k_world_width; ++i)
                        {
                            // Check for solid block.
                            if (g_game_world_collision_map[y][i] != inverse)
                            {
                                // Calculate the squared distance.
                                i32 const dx { static_cast<i32>(x) - static_cast<i32>(i) };
                                u32 const dx2{ static_cast<u32>(dx * dx) };

                                // Keep the smallest distance.
                                if (dx2 < min) min = dx2;
                            }
                        }

                        // Store the smallest distance.
                        sedt_x[y][x] = min;
                    }
                }
            }
        }

        // Perform the vertical pass plus calculating square roots.
        {
            for (u32 y{ 0 }; y < k_world_height; ++y)
            {
                for (u32 x{ 0 }; x < k_world_width; ++x)
                {
                    // Get the minimum on the x axis.
                    u32 min{ sedt_x[y][x] };

                    // Check for solid block.
                    if (min == 0)
                    {
                        // The map is default initialized to 0, so we don't need to store 0 here.
                    }
                    else
                    {
                        // Go through the rows.
                        for (u32 i{ 0 }; i < k_world_height; ++i)
                        {
                            // Calculate the squared distance.
                            i32 const dy { static_cast<i32>(y) - static_cast<i32>(i) };
                            u32 const dx2{ sedt_x[i][x] };

                            // Calculate the squared length of the hypotenuse.
                            u32 const hyp{ dx2 + static_cast<u32>(dy * dy) };

                            // Keep the smallest hypotenuse.
                            if (hyp < min) min = hyp;
                        }

                        // Calculate the distance (we cap the squared distance at 65535, as higher numbers are not
                        // supported).
                        fixed16_16 const dist{
                            fixed16_16::sqrt(static_cast<u16>((min < 65535) ? min : 65535))
                        };

                        // Store the signed distance.
                        g_game_world_distance_field[y][x] = (inverse ? -dist : dist);
                    }
                }
            }
        }
    }

    // Setup noise textures.

    void compute_white_noise_texture()
    {
        // This noise is probably far higher quality than it needs to be, but whatever, now it's written.

        // Initialize our state. These constants represent the first 128 bits in the initial hash value of SHA256.
        u32 state[4]
        {
            0x6A09E667Ui32,
            0xBB67AE85Ui32,
            0x3C6EF372Ui32,
            0xA54FF53AUi32
        };

        // Setup a pointer so we can write 4 bytes at a time.
        static_assert(k_white_noise_texture_width % 4 == 0);
        u32* ptr{ reinterpret_cast<u32*>(&g_white_noise_texture[0][0]) };

        // Go through every pixel and generate a value using xoshiro128** 1.1 (Written this way for optimal code size).
        // (https://xoshiro.di.unimi.it/xoshiro128starstar.c)
        for (u32 n{ 0 }; n != (k_white_noise_texture_width * k_white_noise_texture_height); n += 4, ++ptr)
        {
            u32 const rand{ _rotl(state[1] * 5, 7) * 9 };

            u32 const t{ state[1] << 9 };
            state[2] ^= state[0];
            state[3] ^= state[1];
            state[1] ^= state[2];
            state[0] ^= state[3];
            state[2] ^= t;
            state[3]  = _rotl(state[3], 11);

            *ptr = rand;
        }
    }

    void compute_fractal_noise_texture()
    {
        // Sample the white noise texture multiple times at various scales and blend together.

        for (u32 y{ 0 }; y < k_fractal_noise_texture_height; ++y)
        {
            for (u32 x{ 0 }; x < k_fractal_noise_texture_width; ++x)
            {
                u8 sum{ 0 };

                for (u8 i{ 0 }; i < 4U; ++i)
                {
                    u8 const scale{ 4U - i };
                    u8 const f{ 1U << scale };

                    u32 const y_i{ y >> scale };
                    u32 const y_f{ y & (f - 1) };

                    u32 const x_i{ x >> scale };
                    u32 const x_f{ x & (f - 1) };

                    u32 const tmp_sum
                        = (g_white_noise_texture[y_i + 0Ui32][x_i + 0Ui32] * (f - y_f) * (f - x_f))
                        + (g_white_noise_texture[y_i + 0Ui32][x_i + 1Ui32] * (f - y_f) * (    x_f))
                        + (g_white_noise_texture[y_i + 1Ui32][x_i + 0Ui32] * (    y_f) * (f - x_f))
                        + (g_white_noise_texture[y_i + 1Ui32][x_i + 1Ui32] * (    y_f) * (    x_f));

                    sum += static_cast<u8>(tmp_sum >> (scale * 2U)) >> (i + 1U);
                }
                
                g_fractal_noise_texture[y][x] = sum;
            }
        }
    }

#if 0
    // Particle pathfinding

    namespace detail
    {
        u8 g_pathfinder_vector_map_visited_flags[k_world_height][k_world_width];
    };

    constinit vec2<fixed16_16> g_particle_pathfinder_vector_map[k_world_height][k_world_width];

    __forceinline void init_particle_pathfinder_vector_map()
    {
        for (u16 y{ 0 }; y < k_world_height; ++y)
        {
            for (u16 x{ 0 }; x < k_world_width; ++x)
            {
                // Check if this pixel is not traversable.
                if (g_game_world_collision_map[y][x])
                {
                    // Use the distance field to store the appropriate gradient for pushing the particle out.

                    auto const left  = (x > 0)                     ? g_game_world_distance_field[y][x - 1] : g_game_world_distance_field[y][x];
                    auto const right = (x < (k_world_width  - 1U)) ? g_game_world_distance_field[y][x + 1] : g_game_world_distance_field[y][x];
                    auto const up    = (y > 0)                     ? g_game_world_distance_field[y - 1][x] : g_game_world_distance_field[y][x];
                    auto const down  = (y < (k_world_height - 1U)) ? g_game_world_distance_field[y + 1][x] : g_game_world_distance_field[y][x];

                    g_particle_pathfinder_vector_map[y][x].x = (right - left) * 4;
                    g_particle_pathfinder_vector_map[y][x].y = ( down -   up) * 4;

                    detail::g_pathfinder_vector_map_visited_flags[y][x] = 0b11;
                }
            }
        }
    }

    bool update_particle_pathfinder_vector_map()
    {
        // Could make the frontier array smaller by making it a circular deque,
        // but the size would then need to be tuned to avoid overflow.
        // It's only 2MB anyway.

        static vec2<u16>  frontier[k_world_width * k_world_height]; 
        static u32        frontier_tail, frontier_head;
        static u8         cur_visited_flag;

        // Initialization.
        if (frontier_tail == 0)
        {
            // Get the player's current position as an integer.
            u16 const start_x{ static_cast<u16>(ifloor(g_player.pos.x)) };
            u16 const start_y{ static_cast<u16>(ifloor(g_player.pos.y)) };

            // Flip between using the first or second bit to check if a pixel has already been visited.
            cur_visited_flag = 1Ui8 + (cur_visited_flag & 1Ui8);

            // Insert the player's current position at the start of the frontier.
            frontier[0]   = vec2<u16>{ start_x, start_y };
            frontier_head = 1;

            // Update the visited map.
            detail::g_pathfinder_vector_map_visited_flags[start_y][start_x] = cur_visited_flag;
        }

        // Setup the step counter, which limits the number of steps we perform per frame.
        constexpr u32 step_limit{ 0x1'0000 };
        u32 steps{ 0 };

        // Loop until we finished the flood fill (or abort due to step limit).
        do
        {
            // Save the current head, as it gets modified when we add new nodes to the frontier.
            u32 const old_frontier_head{ frontier_head };

            // Go through new nodes in the frontier.
            do
            {
                // Get the current node and advance the tail.
                auto const cur = frontier[frontier_tail++];

                // Go through the 8 neighbors (bounds checking is not performed as the map has a border).
                for (u8 i{ 0 }; i < 8; ++i)
                {
                    constexpr vec2<i16> d[8]
                    { 
                        vec2<i16>{  1,  0 },
                        vec2<i16>{  0,  1 },
                        vec2<i16>{ -1,  0 },
                        vec2<i16>{  0, -1 },
                        vec2<i16>{  1, -1 },
                        vec2<i16>{  1,  1 },
                        vec2<i16>{ -1,  1 },
                        vec2<i16>{ -1, -1 }
                    };

                    // Compute the x and y coordinates of the neighbor.
                    u16 const next_x{ static_cast<u16>(static_cast<i16>(cur.x) + d[i].x) };
                    u16 const next_y{ static_cast<u16>(static_cast<i16>(cur.y) + d[i].y) };

                    // Check if traversable and not already visited (non-traversable pixels always fail the visited check).
                    if ((detail::g_pathfinder_vector_map_visited_flags[next_y][next_x] & cur_visited_flag) == 0)
                    {
                        // Make the vector map at the neighbour point towards the current node.
                        g_particle_pathfinder_vector_map[next_y][next_x] = vec2<fixed16_16>{
                            fixed16_16{ -d[i].x },
                            fixed16_16{ -d[i].y }
                        };

                        // Mark as visited.
                        detail::g_pathfinder_vector_map_visited_flags[next_y][next_x] = cur_visited_flag;

                        // Add to the frontier, advancing the head.
                        frontier[frontier_head++] = vec2<u16>{ next_x, next_y };
                    }
                }

                // Increment the step counter.
                ++steps;
            } while (frontier_tail != old_frontier_head);
        } while ((steps < step_limit) && (frontier_tail != frontier_head));

        // Check if we finished.
        if (frontier_tail == frontier_head)
        {
            // Reset.
            frontier_tail = 0;

            // Return true to indicate the distance map is complete.
            return true;
        }

        // Return false to indicate that the distance map is incomplete.
        return false;
    }

    constinit vec3<fixed16_16> g_gradient_map[k_world_height][k_world_width];

    void compute_gradient_map()
    {
        for (u32 y{ 1 }; y < k_world_height - 1; ++y)
        {
            for (u32 x{ 1 }; x < k_world_width - 1; ++x)
            {
                fixed16_16 const dx{ g_particle_pathfinder_vector_map[y][x].x * 2 };
                fixed16_16 const dy{ g_particle_pathfinder_vector_map[y][x].y * 2 };

                fixed16_16 value_x = dx * 12;
                fixed16_16 value_y = dy * 12;

                value_x += fixed16_16{ static_cast<i16>(static_cast<i32>(static_cast<u32>(g_fractal_noise_texture[y - 1][x])) - static_cast<i32>(static_cast<u32>(g_fractal_noise_texture[y + 1][x]))) } * 1;
                value_y += fixed16_16{ static_cast<i16>(static_cast<i32>(static_cast<u32>(g_fractal_noise_texture[y][x + 1])) - static_cast<i32>(static_cast<u32>(g_fractal_noise_texture[y][x - 1]))) } * 1;
                //value.x += (g_game_world_distance_field[y][x + 1] - g_game_world_distance_field[y][x - 1]) * 2;
                //value.y += (g_game_world_distance_field[y + 1][x] - g_game_world_distance_field[y - 1][x]) * 2;

                g_gradient_map[y][x].x = value_x;
                g_gradient_map[y][x].y = value_y;
            }
        }
    }

    void upload_gradient_map()
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, g_gradient_map_texture_id);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, k_world_width, k_world_height, GL_RGB_INTEGER, GL_INT, g_gradient_map);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    }
#endif

    // Setup the background texture.

    void compute_background_texture()
    {
        static vec3<u8> background_texture[k_world_height][k_world_width];

        constexpr u32 brick_width { 16 };
        constexpr u32 brick_height{ brick_width / 2 };

        for (u32 y{ 0 }; y < k_world_height; ++y)
        {
            u32 const bi_y{ y / brick_height };
            u32 const bf_y{ y % brick_height };

            u32 const offset_x{ ((bi_y & 1) == 1) ? (brick_width / 2) : 0 };

            for (u32 x{ 0 }; x < k_world_width; ++x)
            {
                if (!g_game_world_collision_map[y][x])
                {
                    u32 const bi_x{ (x + offset_x) / brick_width  };
                    u32 const bf_x{ (x + offset_x) % brick_width  };
                
                    if ((bf_x < 1) || (bf_y < 1))
                    {
                        background_texture[y][x] = vec3<u8>{ static_cast<u8>(g_white_noise_texture[y][x] / 16) };
                    }
                    else
                    {
                        u8 brick_color{ static_cast<u8>(40Ui8 + (g_white_noise_texture[bi_y][bi_x] / 3)) };
                        brick_color  = static_cast<u8>(((static_cast<u16>(brick_color) << 2) - brick_color + g_fractal_noise_texture[y][x]) / 6);
                        brick_color &= static_cast<u8>(~3Ui8);
                        brick_color |= (g_white_noise_texture[y][x] & 1);

                        background_texture[y][x] = vec3<u8>{ brick_color };
                    }
                }
                else
                {
#if 0
                    // TODO: Combine some texture with this alpha
                    i16 const asdf = ifloor(max(g_game_world_distance_field[y][x] + 15, fixed16_16{ 0 }) * 3);
                    background_texture[y][x].x = static_cast<u8>((asdf * asdf) / 8);
#endif
                }
            }
        }

        glGenTextures(1, &g_background_texture_id);
        glBindTexture(GL_TEXTURE_RECTANGLE, g_background_texture_id);
        glTexImage2D (GL_TEXTURE_RECTANGLE, 0, GL_RGB8UI, k_world_width, k_world_height, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, background_texture);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    }

    // Initialization.

    __forceinline void init()
    {
        G21_DEBUG_PRINT("#DEBUG: Initializing.\n");

        init_window();
        init_gl(); 

        // This is hack to display the "Loading. . ." message.
        {
            MSG msg;
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) != 0)
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        G21_DEBUG_PRINT("#DEBUG: Computing textures.\n");

        compute_game_world_collision_map();

        compute_player_collision_map();

        compute_game_world_distance_field(0);
        compute_game_world_distance_field(1);

#if 0
        init_particle_pathfinder_vector_map();
#endif

        compute_white_noise_texture();
        compute_fractal_noise_texture();

        compute_background_texture();
    }

#if 0
    void emit_particles()
    {
        glUseProgram(g_compute_particle_emitter_program_id);

        glUniform1i(0, g_particle_count);                   // particle_count
        glUniform2i(1, 200 << 16, 400 << 16);               // emit_location

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_particle_buffer_id);

        glDispatchCompute((g_particle_count + (LOCAL_SIZE_X - 1)) / LOCAL_SIZE_X, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

        glUseProgram(0);
    }
#endif

    void update_camera()
    {
        // Calculate where we would like the center of the camera to be.

        u16 const desired_center_x{ static_cast<u16>(static_cast<u16>(ifloor(g_player.pos.x)) + (player::k_width  / 2Ui16)) };
        u16 const desired_center_y{ static_cast<u16>(static_cast<u16>(ifloor(g_player.pos.y)) + (player::k_height / 2Ui16)) };

        // Adjust so it doesn't go beyond the edges.

        i16 camera_left = static_cast<i16>(static_cast<i16>(desired_center_x) - static_cast<i16>(camera::k_width / 2Ui16));
        if (camera_left < 0)
        {
            camera_left = 0;
        }
        else if (static_cast<u16>(camera_left) > (k_world_width - camera::k_width))
        {
            camera_left = static_cast<i16>(k_world_width - camera::k_width);
        }

        i16 camera_top = static_cast<i16>(static_cast<i16>(desired_center_y) - static_cast<i16>(camera::k_height / 2Ui16));
        if (camera_top < 0)
        {
            camera_top = 0;
        }
        else if (static_cast<u16>(camera_top) > (k_world_height - camera::k_height))
        {
            camera_top = static_cast<i16>(k_world_height - camera::k_height);
        }

        // Save the origin (top-left corner).
        g_camera.x = static_cast<u16>(camera_left);
        g_camera.y = static_cast<u16>(camera_top);
    }

    // Collision detection.

    void collision_sweep_test()
    {
        // Get current pixel position.
        i16 const start_x{ ifloor(g_player.pos.x) };
        i16 const start_y{ ifloor(g_player.pos.y) };

        // Calculate the desired next pixel position.
        i16 const end_x{ ifloor(g_player.pos.x + g_player.vel.x) };
        i16 const end_y{ ifloor(g_player.pos.y + g_player.vel.y) };

        // Exit early if no visible movement.
        if ((end_x == start_x) && (end_y == start_y)) return;

        // Get the difference between start and end position.
        i16 diff_x = end_x - start_x;
        i16 diff_y = end_y - start_y;

        // Calculate the step directions on each axis and the absolutes of the differences.
        i16 step_x = 1;
        if (diff_x < 0)
        {
            diff_x = -diff_x;
            step_x = -1;
        }
        i16 step_y = 1;
        if (diff_y < 0)
        {
            diff_y = -diff_y;
            step_y = -1;
        }

        // Setup flags to track whether a collision occured on the x and/or y axis.
        bool collide_x{ false };
        bool collide_y{ false };

        // Walk along the player collision map and check for collisions and handle them
        u16 x = start_x, y = start_y;
        for (u16 ix{ 0 }, iy{ 0 }; (ix < static_cast<u16>(diff_x)) || (iy < static_cast<u16>(diff_y));)
        {
            // Check if we want to take a step on the x-axis.
            if ((((ix << 1) | 1Ui16) * static_cast<u16>(diff_y)) < (((iy << 1) | 1Ui16) * static_cast<u16>(diff_x)))
            {
                // Advance.
                x += step_x;
                ++ix;

                // Check for collision.
                if (g_player_collision_map[y][x])
                {
                    // Save that a collision occurred on the x-axis.
                    collide_x = true;

                    // Step back.
                    x -= step_x;

                    // Check if flying.
                    if (g_player.flying)
                    {
                        // Reverse x direction and reduce speed by half.
                        step_x = -step_x;
                        g_player.vel.x = -g_player.vel.x / 2;
                    }
                    else
                    {
                        // Stop stepping along the x-axis and set horizontal velocity to 0.
                        diff_x = ix;
                        g_player.vel.x = fixed16_16{ 0 };
                    }
                }

                // Check if we will be flying.
                if (!g_player.flying && !g_player_collision_map[y + 1][x])
                {
                    //Update the state to flying
                    g_player.flying  = true;
                    g_player.sliding = false;
                }
            }
            // We want to take a step on the y-axis.
            else
            {
                // Advance.
                y += step_y;
                ++iy;

                // Check for collision.
                if (g_player_collision_map[y][x])
                {
                    // Save that a collision occurred on the y-axis.
                    collide_y = true;

                    // Check if we're moving up.
                    if (step_y < 0)
                    {
                        // Step backwards.
                        y += 1;

                        // Stop stepping along the y-axis and set vertical velocity to 0.
                        diff_y = iy;
                        g_player.vel.y = fixed16_16{ 0 };
                    }
                    else
                    {
                        // We are not flying anymore, unless sliding causes us to fall off an edge.
                        g_player.flying = false;

                        // Check if it's possible to slide left (We only slide off edges if we are already sliding).
                        if (!g_player_collision_map[y][x - 1] && (g_player_collision_map[y + 1][x - 1] || g_player.sliding))
                        {
                            // Although technically not a collision on the x-axis, we are adjusting the coordinate so
                            // pretend that a collision occurred on the x-axis.
                            collide_x = true;

                            // Move to the left.
                            x -= 1;
                            ++ix;

                            // Check if we will be flying.
                            if (!g_player_collision_map[y + 1][x])
                            {
                                // Add some horizontal velocity as we fly off.
                                g_player.vel.x = -g_player.vel.y / 2;

                                // Update the state to flying.
                                g_player.flying  = true;
                                g_player.sliding = false;
                            }
                            else
                            {
                                // Update the state to sliding.
                                g_player.sliding = true;
                            }
                        }
                        // Check if it's possible to slide right (We only slide off edges if we are already sliding).
                        else if (!g_player_collision_map[y][x + 1] && (g_player_collision_map[y + 1][x + 1] || g_player.sliding))
                        {
                            // Although technically not a collision on the x-axis, we are adjusting the coordinate so
                            // pretend that a collision occurred on the x-axis.
                            collide_x = true;

                            // Move to the right.
                            x += 1;
                            ++ix;

                            // Check if we will be flying.
                            if (!g_player_collision_map[y + 1][x])
                            {
                                // Add some horizontal velocity as we fly off.
                                g_player.vel.x = g_player.vel.y / 2;

                                // Update the state to flying.
                                g_player.flying  = true;
                                g_player.sliding = false;
                            }
                            else
                            {
                                // Update the state to sliding.
                                g_player.sliding = true;
                            }
                        }
                        // We have landed on something flat.
                        else
                        {
                            // Step back.
                            y -= step_y;

                            // Stop both vertical and horizontal movement.
                            g_player.vel.x = fixed16_16{ 0 };
                            g_player.vel.y = fixed16_16{ 0 };

                            // We are not sliding anymore.
                            g_player.sliding = false;

                            // Exit the loop.
                            break;
                        }
                    }
                }
            }
        }

        // Check if a collision occurred on the x-axis.
        if (collide_x)
        {
            // Save the adjusted x coordinate.
            g_player.pos.x = fixed16_16{ static_cast<i16>(x) };
        }
        else
        {
            // Updated based on velocity without truncating.
            g_player.pos.x += g_player.vel.x;
        }

        // Check if a collision occured on the y-axis.
        if (collide_y)
        {
            // Save the adjusted y coordinate.
            g_player.pos.y = fixed16_16{ static_cast<i16>(y) };
        }
        else
        {
            // Updated based on velocity without truncating.
            g_player.pos.y += g_player.vel.y;
        }
    }



    void pre_render_update()
    {
        static i16 jump_charge;

#if 0
        // TODO: Remove.
        if (g_input.Space)
        {
            g_particle_init = true;
        }
#endif

        // Check if player is holding the A button and not the D button.
        if (g_input.A && !g_input.D)
        {
            g_player.facing = 1;
        }
        // Check if the player is holding the D button and not the A button.
        else if (!g_input.A && g_input.D)
        {
            g_player.facing = 0;
        }
        // Otherwise we keep the current facing direction.

        // Check that the player is not flying (jumping/falling) or sliding.
        if (!g_player.flying && !g_player.sliding)
        {
            // Zero out any previous movement.
            g_player.vel.x = fixed16_16{ 0 };
            g_player.vel.y = fixed16_16{ 0 };

            // Check that the player is not holding the jump key (W).
            if (!g_input.W)
            {
                // Check if the player released the jump key.
                if (jump_charge > 0)
                {
                    if (jump_charge > 8) jump_charge -= 8;
                    else jump_charge = 0;

                    i16 const vel = ifloor(fixed16_16{ 6i16 } + ((fixed16_16{ 4045i16 } / 32767i16) * jump_charge));
                    g_player.vel.x = (((fixed16_16{ 21063i16 } / 32767i16) - ((fixed16_16{ 375i16 } / 32767i16) * jump_charge)) * vel) * (static_cast<i16>(static_cast<i8>(g_input.D) - static_cast<i8>(g_input.A)));
                    g_player.vel.y = -((fixed16_16{ 25101i16 } / 32767i16) + ((fixed16_16{ 191i16 } / 32767i16) * jump_charge)) * vel;

                    g_player.flying = true;

                    // Reset the jump charge.
                    jump_charge = 0;
                }
                else
                {
                    //Add horizontal movement if the player holds exclusively the left (A) key or the right (D) key
                    g_player.vel.x += fixed16_16{ static_cast<i16>(static_cast<i8>(g_input.D) - static_cast<i8>(g_input.A)) * 2 };
                }
            }
            else
            {
                // TODO: Different animation for charging

                ++jump_charge;
                if (jump_charge > 35)
                {
                    jump_charge = 35;
                }
            }
        }
        // Player is flying (jumping/falling).
        else
        {
            // Apply gravity.
            g_player.vel.y += k_gravity;
        }

        // Apply drag.
        g_player.vel.y.raw() = (g_player.vel.y.raw() * 99) / 100;

        // Apply some maximum fall speed.
        if (g_player.vel.y > fixed16_16{ 7 })
        {
            g_player.vel.y = fixed16_16{ 7 };
        }

        // Move the player and perform collision tests.
        collision_sweep_test();

        // Move the camera to follow the player.
        update_camera();

#if 0
        //  TODO: Move/Change this.
        if (g_particle_init)
        {
            emit_particles();
            g_particle_init = false;
        }
#endif
    }

#if 0
    // Particle simulation.

    void update_particles()
    {
        glUseProgram(g_compute_particle_updater_program_id);

        glUniform1i(0, g_particle_count);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_atomic_counter_buffer_id);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &g_active_particles);
        GLuint new_counter{ 0 };
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &new_counter);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_particle_buffer_id); 
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_index_buffer_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_atomic_counter_buffer_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, g_gradient_map_texture_id);

        glDispatchCompute((g_particle_count + (LOCAL_SIZE_X - 1)) / LOCAL_SIZE_X, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);

        glUseProgram(0);
    }

    void post_render_update()
    {
        // There is no need to worry about making sure this map uses the most recent data.
        // It's more important that we don't delay the rendering.
        if (update_particle_pathfinder_vector_map())
        {
            compute_gradient_map();
            upload_gradient_map ();
        }

        update_particles();
    }

    __forceinline void render_particles()
    {
        // Check if there are any particles to render.
        if (g_active_particles > 0)
        {
            glUseProgram(g_render_program_id);

            glUniform4i(0, g_camera.x, g_camera.y, camera::k_width, camera::k_height);

            glBindBuffer(GL_ARRAY_BUFFER, g_particle_buffer_id);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer_id);

            glVertexAttribIPointer(0, 2, GL_INT, sizeof(cs_particle), nullptr);

            glDrawElements(GL_POINTS, g_active_particles, GL_UNSIGNED_INT, nullptr);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glUseProgram(0);
        }
    }
#endif

    // Rendering.

    __forceinline void render_background()
    {
        glUseProgram(g_background_renderer_program_id);
        
        glUniform4i(0, g_camera.x, g_camera.y, camera::k_width, camera::k_height);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, g_background_texture_id);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    __forceinline void render_framebuffer()
    {
        glViewport(g_viewport.x, g_viewport.y, g_viewport.z, g_viewport.w);

        glUseProgram(g_upscaler_program_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_framebuffer_texture_id);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void render()
    {
        // Bind the framebuffer we use to render to our internal fixed resolution.
        glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer_id);
        glViewport(0, 0, camera::k_width, camera::k_height);

        // Render the background.
        glBlendFunc(GL_ONE, GL_ZERO);
        render_background();

        // Render the sprites.
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        push_sprite(vec2<fixed16_16>{ g_player.pos.x - 2, g_player.pos.y - 15 }, vec2<u8>{ 16, 16 }, 1);
        push_sprite(vec2<fixed16_16>{ g_player.pos.x - 2, g_player.pos.y +  1 }, vec2<u8>{ 16, 16 }, (g_player.facing ? 3 : 2));
        render_sprites();

        // Unbind the framebuffer, in preparation for upscaling the image to the target resolution.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Upscale by rendering our framebuffer to the default framebuffer.
        render_framebuffer();

#if 0
        // Render the particles at full resolution.
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        render_particles();
#endif

        // Present.
        SwapBuffers(g_hDC);
        //glFinish();
    }

    // Game loop.

    __declspec(noreturn, noinline) void loop()
    {
        G21_DEBUG_PRINT("#DEBUG: Entering main loop.\n");

        // Pin the thread to core #2.
        SetThreadAffinityMask(GetCurrentThread(), 2);

        // Get the frequency of the counter we use to measure the elapsed time.
        auto const clock_frequency{ []() -> u64
        {
            LARGE_INTEGER result;
            QueryPerformanceFrequency(&result);
            return static_cast<u64>(result.QuadPart);
        }() };
        
        // Initialize our clock.
        u64 acc{ 0 };
        LARGE_INTEGER old_time, new_time;
        QueryPerformanceCounter(&old_time);

        // Loop until window is closed (the event handler calls ExitProcess).
        while (true)
        {
            // Check if there are any window messages to handle.
            if (MSG msg; PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) != 0)
            {
                // Handle the message.
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            else
            {
                // Get the elapsed time.
                QueryPerformanceCounter(&new_time);
                acc += u64_multiply_by_60(static_cast<u64>(new_time.QuadPart - old_time.QuadPart));
                old_time = new_time;

                // Check if enough time has passed for 1 frame.
                if (acc >= clock_frequency)
                {
                    acc -= clock_frequency;

                    pre_render_update();

                    render();

                    //post_render_update();
                }
                
                // Yield the rest of our CPU timeslice.
                //Sleep(0);
            }
        }
    }

    // SetProcessDPIAware.

    __forceinline void set_process_dpi_aware()
    {
        G21_DEBUG_PRINT("#DEBUG: Checking for SetProcessDPIAware.\n");

        // Load the user32 dll.
        HMODULE const user32_dll{ LoadLibraryA("user32.dll") };

        #ifdef _DEBUG
        // Might as well test it in debug mode.
        if (user32_dll == nullptr)
        {
            G21_DEBUG_PRINT("#DEBUG: user32.dll is missing???\n");
            __fastfail(FAST_FAIL_FATAL_APP_EXIT);
        }
        #else
        // This really shouldn't be null. Silence the analyser in release mode.
        __analysis_assume(user32_dll != nullptr);
        #endif

        // Check for the presence of SetProcessDPIAware.
        auto const proc = reinterpret_cast<BOOL(*)()>(GetProcAddress(user32_dll, "SetProcessDPIAware"));
        if (proc)
        {
            // Call it.
            G21_DEBUG_PRINT("#DEBUG: Invoking SetProcessDPIAware.\n");
            proc();
        }
        else
        {
            G21_DEBUG_PRINT("#DEBUG: SetProcessDPIAware not found, ignoring.\n");
        }

        //FreeLibrary(user32_dll);
    }
}

// Our program entry-point.
extern "C" __declspec(noreturn) void __cdecl _main()
{
    G21_DEBUG_INIT;

    G21_DEBUG_PRINT("#DEBUG: Entered program entry-point.\n");

    // Tell Windows that we want the true resolution of the screen, and do not want Windows to scale stuff for us.
    set_process_dpi_aware();

    // Perform all necessary initialization.
    init();

    // Run the game loop (Does not return).
    loop();
}
