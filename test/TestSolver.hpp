#pragma once
#include "TestCommon.hpp"

#ifdef NANO_APPLE_ACCELERATE_SUPPORT
#include <Accelerate/Accelerate.h>
#endif

void t_apple_accelerate()
{
#ifdef NANO_APPLE_ACCELERATE_SUPPORT
    // apple example
    int rowIndices[]    = {   0,    1,   3,   0,   1,    2,   3,   1,   2 };
    double values[]     = { 2.0, -0.2, 2.5, 1.0, 3.2, -0.1, 1.1, 1.4, 0.5 };
 
    long columnStarts[] = {   0,              3,                   7,        9};
 
    SparseMatrix_Double A = {
        .structure = {
        .rowCount     = 4,
        .columnCount  = 3,
        .columnStarts = columnStarts,
        .rowIndices   = rowIndices,
        // Matrix meta-data.
        .attributes = {
            .kind = SparseOrdinary,          
        },
        .blockSize = 1
        },
        .data = values
    };

    double bValues[] = { 1.200, 1.013, 0.205, -0.172 };
    DenseVector_Double b = {
    .count = 4,    
    .data = bValues 
    };
 
    double xValues[] = {   0.0,   0.0,   0.0 };
    DenseVector_Double x = {
        .count = 3,     
        .data = xValues 
    };

    __auto_type status = SparseSolve( SparseLSMR(), A, b, x, SparsePreconditionerDiagScaling);
    if(status!=SparseIterativeConverged) {    
        printf("Failed to converge. Returned with error %d\n", status);}
    else {
        printf("x = "); 
        for(int i=0; i<x.count; i++) 
            printf(" %.2f", x.data[i]); printf("\n");
    }
#endif
}

test_suite * create_nano_heat_solver_test_suite()
{
    test_suite * solver_suite = BOOST_TEST_SUITE("s_heat_solver_test");
    //
    solver_suite->add(BOOST_TEST_CASE(&t_apple_accelerate));
    //
    return solver_suite;
}