#include "OctaveInterface.h"

#include <stdio.h>

#include <shogun/ui/SGInterface.h>
#include <shogun/lib/ShogunException.h>
#include <shogun/lib/io.h>
#include <shogun/lib/memory.h>
#include <shogun/base/init.h>

#ifdef HAVE_PYTHON
#include <dlfcn.h>
#include "../python/PythonInterface.h"
#endif

void octave_print_message(FILE* target, const char* str)
{
	fprintf(target, "%s", str);
}

void octave_print_warning(FILE* target, const char* str)
{
	if (target==stdout)
		::warning(str);
	else
		fprintf(target, "%s", str);
}

void octave_print_error(FILE* target, const char* str)
{
	if (target!=stdout)
		fprintf(target, "%s", str);
}

void octave_cancel_computations(bool &delayed, bool &immediately)
{
}

extern CSGInterface* interface;

COctaveInterface::COctaveInterface(octave_value_list prhs, int32_t nlhs)
: CSGInterface()
{
	reset(prhs, nlhs);

#ifdef HAVE_PYTHON
	Py_Initialize();
	import_array();
#endif
}

COctaveInterface::~COctaveInterface()
{
#ifdef HAVE_PYTHON
	Py_Finalize();
#endif
	exit_shogun();
}

void COctaveInterface::reset(octave_value_list prhs, int32_t nlhs)
{
	CSGInterface::reset();

	m_nlhs=nlhs;
	m_nrhs=prhs.length();
	m_lhs=octave_value_list();
	m_rhs=prhs;
}

/** get functions - to pass data from the target interface to shogun */


/// get type of current argument (does not increment argument counter)
IFType COctaveInterface::get_argument_type()
{
	octave_value arg=m_rhs(m_rhs_counter);

	if (arg.is_char_matrix())
		return STRING_CHAR;
	else if (arg.is_uint8_type() && arg.is_matrix_type())
		return STRING_BYTE;

	if (arg.is_sparse_type())
	{
		if (arg.is_uint8_type())
			return SPARSE_BYTE;
		else if (arg.is_char_matrix())
			return SPARSE_CHAR;
		else if (arg.is_int32_type())
			return SPARSE_INT;
		else if (arg.is_double_type())
			return SPARSE_REAL;
		else if (arg.is_int16_type())
			return SPARSE_SHORT;
		else if (arg.is_single_type())
			return SPARSE_SHORTREAL;
		else if (arg.is_uint16_type())
			return SPARSE_WORD;
		else
			return UNDEFINED;
	}
	else if (arg.is_matrix_type())
	{
		if (arg.is_uint32_type())
			return DENSE_INT;
		else if (arg.is_double_type())
			return DENSE_REAL;
		else if (arg.is_int16_type())
			return DENSE_SHORT;
		else if (arg.is_single_type())
			return DENSE_SHORTREAL;
		else if (arg.is_uint16_type())
			return DENSE_WORD;
	}
	else if (arg.is_cell())
	{
		Cell c = arg.cell_value();

		if (c.nelem()>0)
		{
			if (c.elem(0).is_char_matrix() && c.elem(0).rows()==1)
				return STRING_CHAR;
			else if (c.elem(0).is_uint8_type() && c.elem(0).rows()==1)
				return STRING_BYTE;
			else if (c.elem(0).is_int32_type() && c.elem(0).rows()==1)
				return STRING_INT;
			else if (c.elem(0).is_int16_type() && c.elem(0).rows()==1)
				return STRING_SHORT;
			else if (c.elem(0).is_uint16_type() && c.elem(0).rows()==1)
				return STRING_WORD;
		}
	}

	return UNDEFINED;
}


int32_t COctaveInterface::get_int()
{
	const octave_value i=get_arg_increment();
	if (!i.is_real_scalar())
		SG_ERROR("Expected Scalar Integer as argument %d\n", m_rhs_counter);

	double s=i.double_value();
	if (s-CMath::floor(s)!=0)
		SG_ERROR("Expected Integer as argument %d\n", m_rhs_counter);

	return int32_t(s);
}

float64_t COctaveInterface::get_real()
{
	const octave_value f=get_arg_increment();
	if (!f.is_real_scalar())
		SG_ERROR("Expected Scalar Float as argument %d\n", m_rhs_counter);

	return f.double_value();
}

bool COctaveInterface::get_bool()
{
	const octave_value b=get_arg_increment();
	if (b.is_bool_scalar())
		return b.bool_value();
	else if (b.is_real_scalar())
		return (b.double_value()!=0);
	else
		SG_ERROR("Expected Scalar Boolean as argument %d\n", m_rhs_counter);

	return false;
}

char* COctaveInterface::get_string(int32_t& len)
{
	const octave_value s=get_arg_increment();
	if (!s.is_string())
		SG_ERROR("Expected String as argument %d\n", m_rhs_counter);

	std::string std_str=s.string_value();
	const char* str= std_str.c_str();
	len=std_str.length();
	ASSERT(str && len>0);

	char* cstr=new char[len+1];
	memcpy(cstr, str, len+1);
	cstr[len]='\0';

	return cstr;
}

#define GET_VECTOR(function_name, oct_type_check, oct_type, oct_converter, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(sg_type*& vec, int32_t& len)						\
{																					\
	const octave_value mat_feat=get_arg_increment();								\
	if (!mat_feat.is_matrix_type() || !mat_feat.oct_type_check() || mat_feat.rows()!=1)	\
		SG_ERROR("Expected " error_string " Vector as argument %d\n", m_rhs_counter); \
																					\
	oct_type m = mat_feat.oct_converter();											\
	len = m.cols();																	\
	vec=new sg_type[len];															\
																					\
	for (int32_t i=0; i<len; i++)														\
			vec[i]= (sg_type) m(i);													\
}
GET_VECTOR(get_byte_vector, is_uint8_type, uint8NDArray, uint8_array_value, uint8_t, uint8_t, "Byte")
GET_VECTOR(get_char_vector, is_char_matrix, charMatrix, char_matrix_value, char, char, "Char")
GET_VECTOR(get_int_vector, is_int32_type, int32NDArray, int32_array_value, int32_t, int32_t, "Integer")
GET_VECTOR(get_short_vector, is_int16_type, int16NDArray, int16_array_value, int16_t, int16_t, "Short")
GET_VECTOR(get_shortreal_vector, is_single_type, Matrix, matrix_value, float32_t, float32_t, "Single Precision")
GET_VECTOR(get_real_vector, is_double_type, Matrix, matrix_value, float64_t, float64_t, "Double Precision")
GET_VECTOR(get_word_vector, is_uint16_type, uint16NDArray, uint16_array_value, uint16_t, uint16_t, "Word")
#undef GET_VECTOR


#define GET_MATRIX(function_name, oct_type_check, oct_type, oct_converter, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(sg_type*& matrix, int32_t& num_feat, int32_t& num_vec) \
{																					\
	const octave_value mat_feat=get_arg_increment();								\
	if (!mat_feat.is_matrix_type() || !mat_feat.oct_type_check())						\
		SG_ERROR("Expected " error_string " Matrix as argument %d\n", m_rhs_counter); \
																					\
	oct_type m = mat_feat.oct_converter();												\
	num_vec = m.cols();																\
	num_feat = m.rows();															\
	matrix=new sg_type[num_vec*num_feat];											\
																					\
	for (int32_t i=0; i<num_vec; i++)													\
		for (int32_t j=0; j<num_feat; j++)												\
			matrix[i*num_feat+j]= (sg_type) m(j,i);									\
}
GET_MATRIX(get_byte_matrix, is_uint8_type, uint8NDArray, uint8_array_value, uint8_t, uint8_t, "Byte")
GET_MATRIX(get_char_matrix, is_char_matrix, charMatrix, char_matrix_value, char, char, "Char")
GET_MATRIX(get_int_matrix, is_int32_type, int32NDArray, int32_array_value, int32_t, int32_t, "Integer")
GET_MATRIX(get_short_matrix, is_int16_type, int16NDArray, int16_array_value, int16_t, int16_t, "Short")
GET_MATRIX(get_shortreal_matrix, is_single_type, Matrix, matrix_value, float32_t, float32_t, "Single Precision")
GET_MATRIX(get_real_matrix, is_double_type, Matrix, matrix_value, float64_t, float64_t, "Double Precision")
GET_MATRIX(get_word_matrix, is_uint16_type, uint16NDArray, uint16_array_value, uint16_t, uint16_t, "Word")
#undef GET_MATRIX

#define GET_NDARRAY(function_name, oct_type_check, oct_type, oct_converter, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(sg_type*& array, int32_t*& dims, int32_t& num_dims)	\
{																					\
	const octave_value mat_feat=get_arg_increment();								\
	if (!mat_feat.is_matrix_type() || !mat_feat.oct_type_check())					\
		SG_ERROR("Expected " error_string " ND Array as argument %d\n", m_rhs_counter); \
																					\
	num_dims = (int32_t) mat_feat.ndims();											\
	dim_vector dimvec = mat_feat.dims();											\
																					\
	dims=new int32_t[num_dims];														\
	for (int32_t d=0; d<num_dims; d++)												\
		dims[d]=(int32_t) dimvec(d);												\
																					\
	oct_type m = mat_feat.oct_converter();											\
	int64_t total_size=m.nelem();													\
																					\
	array=new sg_type[total_size];													\
	for (int64_t i=0; i<total_size; i++)											\
		array[i]= (sg_type) m(i);													\
}
GET_NDARRAY(get_byte_ndarray, is_uint8_type, uint8NDArray, uint8_array_value, uint8_t, uint8_t, "Byte")
GET_NDARRAY(get_char_ndarray, is_char_matrix, charMatrix, char_matrix_value, char, char, "Char")
GET_NDARRAY(get_int_ndarray, is_int32_type, int32NDArray, int32_array_value, int32_t, int32_t, "Integer")
GET_NDARRAY(get_short_ndarray, is_int16_type, int16NDArray, int16_array_value, int16_t, int16_t, "Short")
GET_NDARRAY(get_shortreal_ndarray, is_single_type, Matrix, matrix_value, float32_t, float32_t, "Single Precision")
GET_NDARRAY(get_real_ndarray, is_double_type, NDArray, array_value, float64_t, float64_t, "Double Precision")
GET_NDARRAY(get_word_ndarray, is_uint16_type, uint16NDArray, uint16_array_value, uint16_t, uint16_t, "Word")
#undef GET_NDARRAY

void COctaveInterface::get_real_sparsematrix(TSparse<float64_t>*& matrix, int32_t& num_feat, int32_t& num_vec)
{
	const octave_value mat_feat=get_arg_increment();
	if (!mat_feat.is_sparse_type() || !(mat_feat.is_double_type()))
		SG_ERROR("Expected Sparse Double Matrix as argument %d\n", m_rhs_counter);

	SparseMatrix sm = mat_feat.sparse_matrix_value ();
	num_vec=sm.cols();
	num_feat=sm.rows();
	int64_t nnz=sm.nelem();

	matrix=new TSparse<float64_t>[num_vec];

	int64_t offset=0;
	for (int32_t i=0; i<num_vec; i++)
	{
		int32_t len=sm.cidx(i+1)-sm.cidx(i);
		matrix[i].vec_index=i;
		matrix[i].num_feat_entries=len;

		if (len>0)
		{
			matrix[i].features=new TSparseEntry<float64_t>[len];

			for (int32_t j=0; j<len; j++)
			{
				matrix[i].features[j].entry=sm.data(offset);
				matrix[i].features[j].feat_index=sm.ridx(offset);
				offset++;
			}
		}
		else
			matrix[i].features=NULL;
	}
	ASSERT(offset=nnz);
}

#define GET_STRINGLIST(function_name, oct_type_check1, oct_type_check2, \
		oct_type, oct_converter, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(T_STRING<sg_type>*& strings, int32_t& num_str, int32_t& max_string_len) \
{																					\
	max_string_len=0;																\
	octave_value arg=get_arg_increment();											\
	if (arg.is_cell())																\
	{																				\
		Cell c = arg.cell_value();													\
		num_str=c.nelem();															\
		ASSERT(num_str>=1);															\
		strings=new T_STRING<sg_type>[num_str];										\
																					\
		for (int32_t i=0; i<num_str; i++)												\
		{																			\
			if (!c.elem(i).oct_type_check1() || !c.elem(i).oct_type_check2()		\
					|| !c.elem(i).rows()==1)				\
				SG_ERROR("Expected String of type " error_string " as argument %d.\n", m_rhs_counter); \
																					\
			oct_type str=c.elem(i).oct_converter();									\
																					\
			int32_t len=str.cols();														\
			if (len>0) 																\
			{ 																		\
				strings[i].length=len; /* all must have same length in octave */ 	\
				strings[i].string=new sg_type[len+1]; /* not zero terminated in octave */ \
				int32_t j; 																\
				for (j=0; j<len; j++) 												\
					strings[i].string[j]=str(0,j); 									\
				strings[i].string[j]='\0'; 											\
				max_string_len=CMath::max(max_string_len, len);						\
			}																		\
			else																	\
			{																		\
				SG_WARNING( "string with index %d has zero length.\n", i+1);		\
				strings[i].length=0;												\
				strings[i].string=NULL;												\
			}																		\
		} 																			\
	} 																				\
	else if (arg.oct_type_check1() && arg.oct_type_check2())						\
	{																				\
		oct_type data=arg.oct_converter();											\
		num_str=data.cols(); 														\
		int32_t len=data.rows(); 														\
		strings=new T_STRING<sg_type>[num_str]; 									\
																					\
		for (int32_t i=0; i<num_str; i++) 												\
		{ 																			\
			if (len>0) 																\
			{ 																		\
				strings[i].length=len; /* all must have same length in octave */ 	\
				strings[i].string=new sg_type[len+1]; /* not zero terminated in octave */ \
				int32_t j; 																\
				for (j=0; j<len; j++) 												\
					strings[i].string[j]=data(j,i);									\
				strings[i].string[j]='\0'; 											\
			} 																		\
			else 																	\
			{ 																		\
				SG_WARNING( "string with index %d has zero length.\n", i+1); 		\
				strings[i].length=0; 												\
				strings[i].string=NULL; 											\
			} 																		\
		} 																			\
		max_string_len=len;															\
	}																				\
	else																			\
	{\
	SG_PRINT("matrix_type: %d\n", arg.is_matrix_type() ? 1 : 0); \
		SG_ERROR("Expected String, got class %s as argument %d.\n",					\
			"???", m_rhs_counter);													\
	}\
}
/* ignore the g++ warning here */
GET_STRINGLIST(get_byte_string_list, is_matrix_type, is_uint8_type, uint8NDArray, uint8_array_value, uint8_t, uint8_t, "Byte")
GET_STRINGLIST(get_char_string_list, is_char_matrix, is_char_matrix, charMatrix, char_matrix_value, char, char, "Char")
GET_STRINGLIST(get_int_string_list, is_matrix_type, is_int32_type, int32NDArray, int32_array_value, int32_t, int32_t, "Integer")
GET_STRINGLIST(get_short_string_list, is_matrix_type, is_int16_type, int16NDArray, int16_array_value, int16_t, int16_t, "Short")
GET_STRINGLIST(get_word_string_list, is_matrix_type, is_uint16_type, uint16NDArray, uint16_array_value, uint16_t, uint16_t, "Word")
#undef GET_STRINGLIST


/** set functions - to pass data from shogun to Octave */

void COctaveInterface::set_int(int32_t scalar)
{
	octave_value o(scalar);
	set_arg_increment(o);
}

void COctaveInterface::set_real(float64_t scalar)
{
	octave_value o(scalar);
	set_arg_increment(o);
}

void COctaveInterface::set_bool(bool scalar)
{
	octave_value o(scalar);
	set_arg_increment(o);
}


#define SET_VECTOR(function_name, oct_type, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(const sg_type* vec, int32_t len)				\
{																				\
	oct_type mat=oct_type(dim_vector(1, len));									\
																				\
	for (int32_t i=0; i<len; i++)													\
			mat(i) = (if_type) vec[i];											\
																				\
	set_arg_increment(mat);														\
}
SET_VECTOR(set_byte_vector, uint8NDArray, uint8_t, uint8_t, "Byte")
SET_VECTOR(set_char_vector, charMatrix, char, char, "Char")
SET_VECTOR(set_int_vector, int32NDArray, int32_t, int32_t, "Integer")
SET_VECTOR(set_short_vector, int16NDArray, int16_t, int16_t, "Short")
SET_VECTOR(set_shortreal_vector, Matrix, float32_t, float32_t, "Single Precision")
SET_VECTOR(set_real_vector, Matrix, float64_t, float64_t, "Double Precision")
SET_VECTOR(set_word_vector, uint16NDArray, uint16_t, uint16_t, "Word")
#undef SET_VECTOR

#define SET_MATRIX(function_name, oct_type, sg_type, if_type, error_string)		\
void COctaveInterface::function_name(const sg_type* matrix, int32_t num_feat, int32_t num_vec) \
{																				\
	oct_type mat=oct_type(dim_vector(num_feat, num_vec));						\
																				\
	for (int32_t i=0; i<num_vec; i++)												\
	{																			\
		for (int32_t j=0; j<num_feat; j++)											\
			mat(j,i) = (if_type) matrix[j+i*num_feat];							\
	}																			\
																				\
	set_arg_increment(mat);														\
}
SET_MATRIX(set_byte_matrix, uint8NDArray, uint8_t, uint8_t, "Byte")
SET_MATRIX(set_char_matrix, charMatrix, char, char, "Char")
SET_MATRIX(set_int_matrix, int32NDArray, int32_t, int32_t, "Integer")
SET_MATRIX(set_short_matrix, int16NDArray, int16_t, int16_t, "Short")
SET_MATRIX(set_shortreal_matrix, Matrix, float32_t, float32_t, "Single Precision")
SET_MATRIX(set_real_matrix, Matrix, float64_t, float64_t, "Double Precision")
SET_MATRIX(set_word_matrix, uint16NDArray, uint16_t, uint16_t, "Word")
#undef SET_MATRIX

void COctaveInterface::set_real_sparsematrix(const TSparse<float64_t>* matrix, int32_t num_feat, int32_t num_vec, int64_t nnz)
{
	SparseMatrix sm((octave_idx_type) num_feat, (octave_idx_type) num_vec, (octave_idx_type) nnz);

	int64_t offset=0;
	for (int32_t i=0; i<num_vec; i++)
	{
		int32_t len=matrix[i].num_feat_entries;
		sm.cidx(i)=offset;
		for (int32_t j=0; j<len; j++)
		{
			sm.data(offset) = matrix[i].features[j].entry;
			sm.ridx(offset) = matrix[i].features[j].feat_index;
			offset++;
		}
	}
	sm.cidx(num_vec) = offset;

	set_arg_increment(sm);
}

#define SET_STRINGLIST(function_name, oct_type, sg_type, if_type, error_string)	\
void COctaveInterface::function_name(const T_STRING<sg_type>* strings, int32_t num_str)	\
{																					\
	if (!strings)																	\
		SG_ERROR("Given strings are invalid.\n");									\
																					\
	Cell c= Cell(dim_vector(num_str));															\
	if (c.nelem()!=num_str)															\
		SG_ERROR("Couldn't create Cell Array of %d strings.\n", num_str);			\
																					\
	for (int32_t i=0; i<num_str; i++)													\
	{																				\
		int32_t len=strings[i].length;													\
		if (len>0)																	\
		{																			\
			oct_type str(dim_vector(1,len));										\
			if (str.cols()!=len)													\
				SG_ERROR("Couldn't create " error_string " String %d of length %d.\n", i, len);	\
																					\
			for (int32_t j=0; j<len; j++)												\
				str(j)= (if_type) strings[i].string[j];								\
			c.elem(i)=str;															\
		}																			\
	}																				\
																					\
	set_arg_increment(c);															\
}
SET_STRINGLIST(set_byte_string_list, int8NDArray, uint8_t, uint8_t, "Byte")
SET_STRINGLIST(set_char_string_list, charNDArray, char, char, "Char")
SET_STRINGLIST(set_int_string_list, int32NDArray, int32_t, int32_t, "Integer")
SET_STRINGLIST(set_short_string_list, int16NDArray, int16_t, int16_t, "Short")
SET_STRINGLIST(set_word_string_list, uint16NDArray, uint16_t, uint16_t, "Word")
#undef SET_STRINGLIST

bool COctaveInterface::cmd_run_python()
{
#ifdef HAVE_PYTHON
	return CPythonInterface::run_python_helper(this);
#else
	return false;
#endif
}

DEFUN_DLD (sg, prhs, nlhs, "shogun.")
{
	try
	{
		if (!interface)
		{
			// init_shogun has to be called before anything else
			// exit_shogun is called upon destruction of the interface (see
			// destructor of COctaveInterface
			init_shogun(&octave_print_message, &octave_print_warning,
					&octave_print_error, &octave_cancel_computations);
			interface=new COctaveInterface(prhs, nlhs);
		}
		else
			((COctaveInterface*) interface)->reset(prhs, nlhs);

		if (!interface->handle())
			SG_SERROR("Unknown command.\n");

		return ((COctaveInterface*) interface)->get_return_values();
	}
	catch (std::bad_alloc)
	{
		SG_SPRINT("Out of memory error.\n");
		return octave_value_list();
	}
	catch (ShogunException e)
	{
		error("%s", e.get_exception_string());
		return octave_value_list();
	}
	catch (...)
	{
		error("%s", "Returning from SHOGUN in error.");
		return octave_value_list();
	}
}
