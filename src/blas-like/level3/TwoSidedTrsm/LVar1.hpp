/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef EL_TWOSIDEDTRSM_LVAR1_HPP
#define EL_TWOSIDEDTRSM_LVAR1_HPP

namespace El {
namespace twotrsm {

template<typename F> 
inline void
LVar1( UnitOrNonUnit diag, Matrix<F>& A, const Matrix<F>& L )
{
    DEBUG_ONLY(
        CallStackEntry cse("twotrsm::LVar1");
        if( A.Height() != A.Width() )
            LogicError("A must be square");
        if( L.Height() != L.Width() )
            LogicError("Triangular matrices must be square");
        if( A.Height() != L.Height() )
            LogicError("A and L must be the same size");
    )
    const Int n = A.Height();
    const Int bsize = Blocksize();

    // Temporary products
    Matrix<F> Y10;

    for( Int k=0; k<n; k+=bsize )
    {
        const Int nb = Min(bsize,n-k);

        const Range<Int> ind0( 0, k    );
        const Range<Int> ind1( k, k+nb );

        auto A00 = LockedView( A, ind0, ind0 );
        auto A10 =       View( A, ind1, ind0 );
        auto A11 =       View( A, ind1, ind1 );

        auto L00 = LockedView( L, ind0, ind0 );
        auto L10 = LockedView( L, ind1, ind0 );
        auto L11 = LockedView( L, ind1, ind1 );

        // Y10 := L10 A00
        Zeros( L10, L10.Height(), A00.Width() );
        Hemm( RIGHT, LOWER, F(1), A00, L10, F(0), Y10 );

        // A10 := A10 inv(L00)'
        Trsm( RIGHT, LOWER, ADJOINT, diag, F(1), L00, A10 );

        // A10 := A10 - 1/2 Y10
        Axpy( F(-1)/F(2), Y10, A10 );

        // A11 := A11 - (A10 L10' + L10 A10')
        Her2k( LOWER, NORMAL, F(-1), A10, L10, F(1), A11 );

        // A11 := inv(L11) A11 inv(L11)'
        twotrsm::LUnb( diag, A11, L11 );

        // A10 := A10 - 1/2 Y10
        Axpy( F(-1)/F(2), Y10, A10 );

        // A10 := inv(L11) A10
        Trsm( LEFT, LOWER, NORMAL, diag, F(1), L11, A10 );
    }
}

template<typename F> 
inline void
LVar1
( UnitOrNonUnit diag, 
  AbstractDistMatrix<F>& APre, const AbstractDistMatrix<F>& LPre )
{
    DEBUG_ONLY(
        CallStackEntry cse("twotrsm::LVar1");
        if( APre.Height() != APre.Width() )
            LogicError("A must be square");
        if( LPre.Height() != LPre.Width() )
            LogicError("Triangular matrices must be square");
        if( APre.Height() != LPre.Height() )
            LogicError("A and L must be the same size");
    )
    const Int n = APre.Height();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();

    DistMatrix<F> A(g), L(g);
    Copy( APre, A, READ_WRITE_PROXY );
    Copy( LPre, L, READ_PROXY );
    
    // Temporary distributions
    DistMatrix<F,STAR,STAR> A11_STAR_STAR(g), L11_STAR_STAR(g),
                            X11_STAR_STAR(g);
    DistMatrix<F,STAR,VR  > A10_STAR_VR(g), L10_STAR_VR(g);
    DistMatrix<F,STAR,MC  > L10_STAR_MC(g);
    DistMatrix<F,MR,  STAR> L10Adj_MR_STAR(g), Z10Adj_MR_STAR(g);
    DistMatrix<F,MC,  STAR> Z10Adj_MC_STAR(g);
    DistMatrix<F,VC,  STAR> L10Adj_VC_STAR(g);
    DistMatrix<F,MR,  MC  > Z10Adj_MR_MC(g);
    DistMatrix<F> Y10(g), Z10Adj(g);

    for( Int k=0; k<n; k+=bsize )
    {
        const Int nb = Min(bsize,n-k);

        const Range<Int> ind0( 0, k    );
        const Range<Int> ind1( k, k+nb );

        auto A00 = LockedView( A, ind0, ind0 );
        auto A10 =       View( A, ind1, ind0 );
        auto A11 =       View( A, ind1, ind1 );

        auto L00 = LockedView( L, ind0, ind0 );
        auto L10 = LockedView( L, ind1, ind0 );
        auto L11 = LockedView( L, ind1, ind1 );

        // Y10 := L10 A00
        L10Adj_MR_STAR.AlignWith( A00 );
        L10.AdjointColAllGather( L10Adj_MR_STAR );
        L10Adj_VC_STAR.AlignWith( A00 );
        L10Adj_VC_STAR = L10Adj_MR_STAR;
        L10_STAR_MC.AlignWith( A00 );
        L10Adj_VC_STAR.AdjointPartialColAllGather( L10_STAR_MC );
        Z10Adj_MC_STAR.AlignWith( A00 );
        Z10Adj_MR_STAR.AlignWith( A00 );
        Zeros( Z10Adj_MC_STAR, k, nb );
        Zeros( Z10Adj_MR_STAR, k, nb );
        symm::LocalAccumulateRL
        ( ADJOINT,
          F(1), A00, L10_STAR_MC, L10Adj_MR_STAR, 
          Z10Adj_MC_STAR, Z10Adj_MR_STAR );
        Z10Adj.AlignWith( A10 );
        Z10Adj.RowSumScatterFrom( Z10Adj_MC_STAR );
        Z10Adj_MR_MC.AlignWith( A10 );
        Z10Adj_MR_MC = Z10Adj;
        Z10Adj_MR_MC.RowSumScatterUpdate( F(1), Z10Adj_MR_STAR );
        Y10.AlignWith( A10 );
        Adjoint( Z10Adj_MR_MC, Y10 );

        // A10 := A10 inv(L00)'
        // This is the bottleneck because A10 only has blocksize rows
        Trsm( RIGHT, LOWER, ADJOINT, diag, F(1), L00, A10 );

        // A10 := A10 - 1/2 Y10
        Axpy( F(-1)/F(2), Y10, A10 );

        // A11 := A11 - (A10 L10' + L10 A10')
        A10_STAR_VR.AlignWith( A10 );
        A10_STAR_VR = A10;
        L10_STAR_VR.AlignWith( A00 );
        L10_STAR_VR = L10;
        Zeros( X11_STAR_STAR, nb, nb );
        Her2k
        ( LOWER, NORMAL,
          F(-1), A10_STAR_VR.Matrix(), L10_STAR_VR.Matrix(), 
          F(0), X11_STAR_STAR.Matrix() );
        A11.SumScatterUpdate( F(1), X11_STAR_STAR );

        // A11 := inv(L11) A11 inv(L11)'
        A11_STAR_STAR = A11;
        L11_STAR_STAR = L11;
        LocalTwoSidedTrsm( LOWER, diag, A11_STAR_STAR, L11_STAR_STAR );
        A11 = A11_STAR_STAR;

        // A10 := A10 - 1/2 Y10
        Axpy( F(-1)/F(2), Y10, A10 );

        // A10 := inv(L11) A10
        A10_STAR_VR = A10;
        LocalTrsm
        ( LEFT, LOWER, NORMAL, diag, F(1), L11_STAR_STAR, A10_STAR_VR );
        A10 = A10_STAR_VR;
    }

    Copy( A, APre, RESTORE_READ_WRITE_PROXY );
}

} // namespace twotrsm
} // namespace El

#endif // ifndef EL_TWOSIDEDTRSM_LVAR1_HPP